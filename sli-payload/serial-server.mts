import { SerialPort } from 'serialport'
import { WebSocketServer } from 'ws'
import { ReadlineParser } from '@serialport/parser-readline'
import * as readline from 'readline'

const wss = new WebSocketServer({ port: 8080 })
let serialPort: SerialPort | null = null

async function getPortFromUser(): Promise<string> {
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  })

  console.log('\nAvailable ports:')
  const ports = await SerialPort.list()
  ports.forEach(port => {
    console.log(`${port.path} - ${port.manufacturer || 'Unknown manufacturer'}`)
  })

  return new Promise((resolve) => {
    rl.question('\nEnter COM port (e.g., COM12): ', (answer) => {
      rl.close()
      resolve(answer.trim())
    })
  })
}

async function connectToPort(portPath: string) {
  try {
    console.log(`Attempting to connect to ${portPath}...`)
    serialPort = new SerialPort({
      path: portPath,
      baudRate: 9600
    })

    const parser = serialPort.pipe(new ReadlineParser({ delimiter: '\n' }))

    // Forward serial data to all connected WebSocket clients
    parser.on('data', (data: string) => {
      console.log('Received data:', data)
      wss.clients.forEach(client => {
        if (client.readyState === 1) {
          client.send(JSON.stringify({ type: 'data', data }))
        }
      })
    })

    serialPort.on('error', (err) => {
      console.error('Serial port error:', err)
    })

    serialPort.on('open', () => {
      console.log(`Successfully connected to ${portPath}`)
    })

  } catch (err) {
    console.error('Failed to connect:', err)
    process.exit(1)
  }
}

// Handle WebSocket connections
wss.on('connection', (ws) => {
  console.log('Client connected')

  // Handle commands from the web interface
  ws.on('message', (message) => {
    try {
      const { command } = JSON.parse(message.toString())
      if (serialPort && serialPort.isOpen) {
        serialPort.write(`${command}\n`)
      }
    } catch (err) {
      console.error('Error processing message:', err)
    }
  })

  ws.on('close', () => {
    console.log('Client disconnected')
  })
})

// Main execution
async function main() {
  try {
    // Check if port was provided as command line argument
    let portPath = process.argv[2]
    
    // If no port provided, ask user
    if (!portPath) {
      portPath = await getPortFromUser()
    }

    // Add COM prefix if not present
    if (!portPath.toLowerCase().startsWith('com')) {
      portPath = `COM${portPath}`
    }

    await connectToPort(portPath)
  } catch (err) {
    console.error('Error:', err)
    process.exit(1)
  }
}

main()

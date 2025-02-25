import { useState, useEffect } from 'react'
import { Line } from 'react-chartjs-2'
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
} from 'chart.js'
import './App.css'

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
)

interface SensorData {
  timestamp: number
  temperature: number
  pressure: number
  altitude: number
  adxl_x: number
  adxl_y: number
  adxl_z: number
  bno_i: number
  bno_j: number
  bno_k: number
  bno_real: number
  current_g: number
  max_g: number
  velocity: number
  max_velocity: number
  battery: number
  mode: number
  rssi: number
  snr: number
}

const modes = [
  { name: 'IDLE', command: 'I' },
  { name: 'RECORD & TRANSMIT', command: 'T' },
  { name: 'RESET', command: 'Z' },
  { name: 'RECORD', command: 'O' },
  { name: 'TRANSMIT', command: 'N' },
  { name: 'DISABLE', command: 'E' }
]

function App() {
  const [sensorData, setSensorData] = useState<SensorData | null>(null)
  const [ws, setWs] = useState<WebSocket | null>(null)
  const [lastReset, setLastReset] = useState<number>(0)
  const [selectedVariable1, setSelectedVariable1] = useState<string>('altitude')
  const [selectedVariable2, setSelectedVariable2] = useState<string>('altitude')
  const [currentMode, setCurrentMode] = useState<string>('IDLE')
  const [dataHistory, setDataHistory] = useState<{
    temperature: { timestamp: number; value: number }[];
    pressure: { timestamp: number; value: number }[];
    altitude: { timestamp: number; value: number }[];
    current_g: { timestamp: number; value: number }[];
    velocity: { timestamp: number; value: number }[];
    rssi: { timestamp: number; value: number }[];
    snr: { timestamp: number; value: number }[];
  }>({
    temperature: [],
    pressure: [],
    altitude: [],
    current_g: [],
    velocity: [],
    rssi: [],
    snr: []
  })

  const variables = [
    { label: 'Temperature (°C)', value: 'temperature' },
    { label: 'Pressure (Pa)', value: 'pressure' },
    { label: 'Altitude (m)', value: 'altitude' },
    { label: 'Current G-Force (G)', value: 'current_g' },
    { label: 'Velocity (m/s)', value: 'velocity' },
    { label: 'RSSI (dBm)', value: 'rssi' },
    { label: 'SNR (dB)', value: 'snr' }
  ]

  useEffect(() => {
    const handleBeforeUnload = (e: BeforeUnloadEvent) => {
      if (dataHistory.temperature.length > 0) {
        e.preventDefault();
        e.returnValue = '';
      }
    };

    window.addEventListener('beforeunload', handleBeforeUnload);
    return () => window.removeEventListener('beforeunload', handleBeforeUnload);
  }, [dataHistory]);

  useEffect(() => {
    const socket = new WebSocket('ws://localhost:8080')

    socket.onopen = () => {
      console.log('Connected to WebSocket server')
    }

    socket.onmessage = (event) => {
      try {
        const { type, data } = JSON.parse(event.data)
        if (type === 'data') {
          const values = data.split(',').map(Number)
          if (values.length === 18) {  
            const newData: SensorData = {
              timestamp: values[0],
              temperature: values[1],
              pressure: values[2],
              altitude: values[3],
              adxl_x: values[4],
              adxl_y: values[5],
              adxl_z: values[6],
              bno_i: values[7],
              bno_j: values[8],
              bno_k: values[9],
              bno_real: values[10],
              current_g: values[11],
              max_g: values[12],
              velocity: values[13],
              max_velocity: values[14],
              battery: values[15],
              mode: values[16],
              rssi: values[17],
              snr: values[18]
            }
            setSensorData(newData)
            switch (newData.mode) {
              case 0: setCurrentMode('SELF_TEST'); break;
              case 1: setCurrentMode('IDLE_1'); break;
              case 2: setCurrentMode('RECORD_ONLY'); break;
              case 3: setCurrentMode('RECORD_AND_TRANSMIT'); break;
              case 4: setCurrentMode('TRANSMIT_ONLY'); break;
              case 5: setCurrentMode('DISABLED'); break;
            }
            setDataHistory(prev => ({
              ...prev,
              temperature: [...prev.temperature, { timestamp: newData.timestamp, value: newData.temperature }],
              pressure: [...prev.pressure, { timestamp: newData.timestamp, value: newData.pressure }],
              altitude: [...prev.altitude, { timestamp: newData.timestamp, value: newData.altitude }],
              current_g: [...prev.current_g, { timestamp: newData.timestamp, value: newData.current_g }],
              velocity: [...prev.velocity, { timestamp: newData.timestamp, value: newData.velocity }],
              rssi: [...prev.rssi, { timestamp: newData.timestamp, value: newData.rssi }],
              snr: [...prev.snr, { timestamp: newData.timestamp, value: newData.snr }]
            }))
          }
        }
      } catch (error) {
        console.error('Error processing message:', error)
      }
    }

    setWs(socket)
    return () => {
      socket.close()
    }
  }, [])

  const chartData = {
    labels: dataHistory[selectedVariable1 as keyof typeof dataHistory].map(
      d => ((d.timestamp - (lastReset || dataHistory[selectedVariable1 as keyof typeof dataHistory][0]?.timestamp || 0)) / 1000).toFixed(1)
    ),
    datasets: [
      {
        label: variables.find(v => v.value === selectedVariable1)?.label || selectedVariable1,
        data: dataHistory[selectedVariable1 as keyof typeof dataHistory].map(d => d.value),
        borderColor: 'rgb(75, 192, 192)',
        tension: 0.1,
        yAxisID: 'y'
      },
      {
        label: variables.find(v => v.value === selectedVariable2)?.label || selectedVariable2,
        data: dataHistory[selectedVariable2 as keyof typeof dataHistory].map(d => d.value),
        borderColor: 'rgb(255, 99, 132)',
        tension: 0.1,
        yAxisID: 'y2'
      }
    ]
  }

  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    interaction: {
      mode: 'index' as const,
      intersect: false,
    },
    scales: {
      x: {
        title: {
          display: true,
          text: 'Time (s)'
        }
      },
      y: {
        type: 'linear' as const,
        display: true,
        position: 'left' as const,
        title: {
          display: true,
          text: variables.find(v => v.value === selectedVariable1)?.label || selectedVariable1
        }
      },
      y2: {
        type: 'linear' as const,
        display: true,
        position: 'right' as const,
        title: {
          display: true,
          text: variables.find(v => v.value === selectedVariable2)?.label || selectedVariable2
        },
        grid: {
          drawOnChartArea: false,
        },
      },
    },
    animation: {
      duration: 0
    }
  }

  const handleModeChange = (command: string) => {
    if (command === 'E') {
      const confirmed = window.confirm('WARNING: Disabling is irreversible.')
      if (!confirmed) {
        return
      }
    }

    if (command === 'Z') {
      const confirmed = window.confirm('WARNING: MAX_G and MAX_VELOCITY will be reset.')
      if (!confirmed) {
        return
      }
      setLastReset(sensorData?.timestamp ?? 0)
      setDataHistory({
        temperature: [],
        pressure: [],
        altitude: [],
        current_g: [],
        velocity: [],
        rssi: [],
        snr: []
      })
    }
    
    if (ws?.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ command }))
      console.log(`Sending mode command: ${ command }`)
    }
  }

  return (
    <div className="container">
      <div className="data-section">
        <div className="card">
          <div className="mode-buttons">
            <div className="mode-buttons-row">
              {modes.slice(0, 3).map((mode) => (
                <button
                  key={mode.name}
                  onClick={() => handleModeChange(mode.command)}
                  className={`mode-button ${mode.name.toLowerCase().replace(/\s+&\s+|\s+/g, '-')}`}
                >
                  {mode.name}
                </button>
              ))}
            </div>
            <div className="mode-buttons-row">
              {modes.slice(3).map((mode) => (
                <button
                  key={mode.name}
                  onClick={() => handleModeChange(mode.command)}
                  className={`mode-button ${mode.name.toLowerCase().replace(/\s+&\s+|\s+/g, '-')}`}
                >
                  {mode.name}
                </button>
              ))}
            </div>
          </div>
        </div>
        <div className="card">
          <div className="data-grid">
            {sensorData ? (
              <>
                <div className="data-item">Timestamp: <code>{sensorData.timestamp}</code></div>
                <div className="data-item">Temperature: <code>{sensorData.temperature.toFixed(2)} °C</code></div>
                <div className="data-item">Pressure: <code>{sensorData.pressure.toFixed(2)} Pa</code></div>
                <div className="data-item">Altitude: <code>{sensorData.altitude.toFixed(2)} m</code></div>
                <div className="data-item">Acceleration (X,Y,Z): <code>({sensorData.adxl_x.toFixed(2)}, {sensorData.adxl_y.toFixed(2)}, {sensorData.adxl_z.toFixed(2)}) m/s²</code></div>
                <div className="data-item">Orientation (i,j,k,real): <code>({sensorData.bno_i.toFixed(2)}, {sensorData.bno_j.toFixed(2)}, {sensorData.bno_k.toFixed(2)}, {sensorData.bno_real.toFixed(2)})</code></div>
                <div className="data-item">Current G-Force: <code>{sensorData.current_g.toFixed(2)} G</code></div>
                <div className="data-item">Max G-Force: <code>{sensorData.max_g.toFixed(2)} G</code></div>
                <div className="data-item">Velocity: <code>{sensorData.velocity.toFixed(2)} m/s</code></div>
                <div className="data-item">Max Velocity: <code>{sensorData.max_velocity.toFixed(2)} m/s</code></div>
                <div className="data-item">Battery: <code>{sensorData.battery.toFixed(2)} V</code></div>
                <div className="data-item">RSSI: <code>{sensorData.rssi.toFixed(2)} dBm</code></div>
                <div className="data-item">SNR: <code>{sensorData.snr.toFixed(2)} dB</code></div>
              </>
            ) : (
              <div className="data-item">Waiting for sensor data...</div>
            )}
          </div>
        </div>
      </div>
      
      <div className="graph-section">
        <div className="variable-selector">
          <div className="variable-select-group">
            <select 
              value={selectedVariable1} 
              onChange={(e) => setSelectedVariable1(e.target.value)}
              className="variable-select"
            >
              {variables.map(variable => (
                <option key={variable.value} value={variable.value}>
                  {variable.label}
                </option>
              ))}
            </select>
            <select 
              value={selectedVariable2} 
              onChange={(e) => setSelectedVariable2(e.target.value)}
              className="variable-select"
            >
              {variables.map(variable => (
                <option key={variable.value} value={variable.value}>
                  {variable.label}
                </option>
              ))}
            </select>
          </div>
        </div>
        <div className="graph-container">
          <Line data={chartData} options={chartOptions} style={{height: '80vh', width: '1050px' }}/>
        </div>
      </div>
    </div>
  )
}

export default App

import { useEffect, useMemo, useState } from 'react';
import {
  AlertTriangle, Boxes, ChevronRight, CircleDot, ClipboardList, Map as MapIcon,
  Pause, Play, Radio, RotateCcw, Route, Square, WifiOff,
} from 'lucide-react';

type Telemetry = { state?: string; message?: string; speed_mps?: string; angular_rps?: string; route_heading?: string };
type MissionTask = { id: string; to: string; state: string };
type Mission = { state?: string; event?: string; mission?: string; robot?: string; current?: string; goal?: string; tasks?: MissionTask[] };
type Status = { schema_version: number; robot_id: string; last_checkpoint: string; telemetry: Telemetry | null; mission: Mission | null };
type Checkpoint = { id: string; x: number; y: number };
type Edge = { from: string; to: string };
type MapData = { floor_size_m: [number, number]; checkpoints: Checkpoint[]; navigation?: { edges: Edge[] } };
type Page = 'overview' | 'warehouse' | 'tasks';

const api = '/api/v1';
const missionState = (status: Status | null) => status?.mission?.state ?? 'waiting';
const pretty = (value?: string) => value ? value.replaceAll('_', ' ') : '—';

function useBridge() {
  const [status, setStatus] = useState<Status | null>(null);
  const [map, setMap] = useState<MapData | null>(null);
  const [connected, setConnected] = useState(false);
  useEffect(() => {
    void fetch(`${api}/map`).then((r) => r.ok ? r.json() : Promise.reject()).then(setMap).catch(() => setMap(null));
    const scheme = location.protocol === 'https:' ? 'wss' : 'ws';
    const socket = new WebSocket(`${scheme}://${location.host}${api}/stream`);
    socket.onopen = () => setConnected(true);
    socket.onmessage = (event) => setStatus(JSON.parse(event.data) as Status);
    socket.onclose = () => setConnected(false);
    socket.onerror = () => setConnected(false);
    return () => socket.close();
  }, []);
  return { status, map, connected };
}

async function sendCommand(command: string) {
  const response = await fetch(`${api}/mission-commands`, {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ command }),
  });
  if (!response.ok) throw new Error('Controller did not accept that command.');
}

function StatePill({ value }: { value: string }) {
  const danger = ['failed', 'recovery_required', 'cancelled'].includes(value);
  const active = ['navigating', 'tracking'].includes(value);
  return <span className={`pill ${danger ? 'danger' : active ? 'active' : ''}`}>{pretty(value)}</span>;
}

function Card({ title, children, className = '' }: { title?: string; children: React.ReactNode; className?: string }) {
  return <section className={`card ${className}`}>{title && <h2>{title}</h2>}{children}</section>;
}

function RobotCard({ status, connected }: { status: Status | null; connected: boolean }) {
  const state = missionState(status);
  return <Card className="robot-card">
    <div className="robot-title"><div className="robot-icon"><Boxes size={22} /></div><div><p className="eyebrow">AMR-01</p><h2>TrackTag AMR</h2></div><StatePill value={state} /></div>
    <div className="metric-grid">
      <div><span>Connection</span><strong className={connected ? 'ok' : 'bad'}>{connected ? 'Live' : 'Offline'}</strong></div>
      <div><span>Confirmed QR</span><strong>{status?.last_checkpoint || 'Waiting'}</strong></div>
      <div><span>Task goal</span><strong>{status?.mission?.goal || 'None'}</strong></div>
      <div><span>Route heading</span><strong>{status?.telemetry?.route_heading || 'Unknown'}</strong></div>
    </div>
  </Card>;
}

function Overview({ status, connected }: { status: Status | null; connected: boolean }) {
  const telemetryState = status?.telemetry?.state ?? 'waiting';
  const alert = ['failed', 'recovery_required'].includes(missionState(status));
  return <div className="page-grid">
    <div className="page-heading"><div><p className="eyebrow">Fleet overview</p><h1>Operations at a glance</h1></div><div className={`connection ${connected ? '' : 'offline'}`}>{connected ? <Radio size={16} /> : <WifiOff size={16} />}{connected ? 'Bridge live' : 'Bridge disconnected'}</div></div>
    <RobotCard status={status} connected={connected} />
    <Card title="Current operation"><div className="operation"><Route size={25} /><div><strong>{status?.mission?.current || 'No active start'} <ChevronRight size={16} /> {status?.mission?.goal || 'No active goal'}</strong><p>{status?.mission?.event || 'Create a task queue to begin.'}</p></div></div></Card>
    <Card title="Controller health"><div className="health-row"><CircleDot className={telemetryState === 'tracking' ? 'ok' : ''} /><div><strong>{pretty(telemetryState)}</strong><p>{status?.telemetry?.message || 'No controller telemetry received.'}</p></div></div></Card>
    <Card title="Alerts"><div className={`alert-row ${alert ? 'alert-danger' : ''}`}><AlertTriangle size={22} /><div><strong>{alert ? 'Operator action required' : 'No active safety alerts'}</strong><p>{alert ? status?.mission?.event : 'Recovery, sensor, and route faults appear here.'}</p></div></div></Card>
  </div>;
}

function Warehouse({ status, map }: { status: Status | null; map: MapData | null }) {
  const points = useMemo(() => new Map((map?.checkpoints ?? []).map((point) => [point.id, point])), [map]);
  const checkpoint = points.get(status?.last_checkpoint ?? '');
  if (!map) return <div className="empty"><MapIcon size={28} /><h2>Waiting for warehouse map</h2><p>Start the bridge with a navigation map to display the live track.</p></div>;
  const [width, height] = map.floor_size_m;
  return <div className="warehouse-page"><div className="page-heading"><div><p className="eyebrow">Live warehouse</p><h1>Track and confirmed position</h1></div><span className="subtle">Position is QR-confirmed, not continuous localization.</span></div>
    <Card className="map-card"><svg className="warehouse-map" viewBox={`${-width / 2} ${-height / 2} ${width} ${height}`} role="img" aria-label="Warehouse track map">
      <rect x={-width / 2} y={-height / 2} width={width} height={height} rx="0.12" className="floor" />
      {(map.navigation?.edges ?? []).map((edge, index) => { const from = points.get(edge.from); const to = points.get(edge.to); return from && to ? <line key={`${edge.from}-${edge.to}-${index}`} x1={from.x} y1={-from.y} x2={to.x} y2={-to.y} className="track" /> : null; })}
      {map.checkpoints.map((point) => <g key={point.id} transform={`translate(${point.x} ${-point.y})`}><circle r="0.075" className="checkpoint" /><text y="-0.13" className="map-label">{point.id.replace('Station ', 'S. ')}</text></g>)}
      {checkpoint && <g transform={`translate(${checkpoint.x} ${-checkpoint.y})`}><circle r="0.16" className="robot-pulse" /><circle r="0.105" className="robot-marker" /><path d="M-.04,.045 L.04,.045 L0,-.06 Z" className="robot-arrow" /></g>}
    </svg><div className="map-legend"><span><i className="legend-robot" /> AMR confirmed at {status?.last_checkpoint || '—'}</span><span><i className="legend-track" /> Configured directed route</span></div></Card>
    <div className="route-summary"><Card title="Current route"><strong>{status?.mission?.current || '—'} <ChevronRight size={16} /> {status?.mission?.goal || '—'}</strong><p>{status?.mission?.event || 'No active route.'}</p></Card><Card title="Navigation confidence"><strong>{checkpoint ? 'Checkpoint confirmed' : 'Awaiting QR'}</strong><p>QR checkpoints validate progress. The marker does not imply continuous pose estimation.</p></Card></div>
  </div>;
}

function Tasks({ status, map }: { status: Status | null; map: MapData | null }) {
  const [queue, setQueue] = useState<string[]>([]);
  const [choice, setChoice] = useState('');
  const [notice, setNotice] = useState('');
  const state = missionState(status);
  const controllerTasks = status?.mission?.tasks ?? [];
  const command = async (value: string) => { try { await sendCommand(value); setNotice(`Requested: ${value}`); } catch (error) { setNotice(error instanceof Error ? error.message : 'Command failed.'); } };
  const createAndStart = async () => { if (!queue.length) return; await command(`create:${queue.join(',')}`); };
  const primary = state === 'navigating' ? ['pause', 'Pause task', Pause]
    : state === 'paused' ? ['resume', 'Resume task', Play]
      : state === 'arrived' ? ['next', `Dispatch next: ${controllerTasks.find((task) => task.state === 'queued')?.to ?? 'complete mission'}`, ChevronRight]
        : ['start', 'Start task', Play];
  const [primaryCommand, primaryLabel, PrimaryIcon] = primary as [string, string, typeof Play];
  return <div className="tasks-page"><div className="page-heading"><div><p className="eyebrow">Task control</p><h1>Plan and operate missions</h1></div><StatePill value={state} /></div>
    <div className="task-layout"><Card title="Build task queue"><label htmlFor="checkpoint">Destination checkpoint</label><div className="add-task"><select id="checkpoint" value={choice} onChange={(event) => setChoice(event.target.value)}><option value="">Select a checkpoint</option>{map?.checkpoints.map((point) => <option key={point.id} value={point.id}>{point.id}</option>)}</select><button disabled={!choice} onClick={() => { setQueue([...queue, choice]); setChoice(''); }}>Add</button></div><ol className="queue">{queue.length ? queue.map((task, index) => <li key={`${task}-${index}`}><span>{index + 1}</span>{task}<button className="icon-button" aria-label={`Remove ${task}`} onClick={() => setQueue(queue.filter((_, item) => item !== index))}>×</button></li>) : <li className="placeholder">No draft destinations.</li>}</ol><button className="primary full" disabled={!queue.length} onClick={() => void createAndStart()}><ClipboardList size={17} /> Create mission</button></Card>
      <Card title="Controller-confirmed queue"><ol className="queue confirmed-queue">{controllerTasks.length ? controllerTasks.map((task, index) => <li key={task.id}><span>{index + 1}</span><div>{task.to}<small>{pretty(task.state)}</small></div></li>) : <li className="placeholder">No mission has been created.</li>}</ol></Card>
      <Card title="Active task" className="active-task-card"><div className="active-task"><span>From</span><strong>{status?.mission?.current || '—'}</strong><span>To</span><strong>{status?.mission?.goal || '—'}</strong><span>Event</span><p>{status?.mission?.event || 'No mission loaded.'}</p></div><div className="control-row"><button className="primary" onClick={() => void command(primaryCommand)}><PrimaryIcon size={17} /> {primaryLabel}</button><button className="secondary" onClick={() => void command('retry')} disabled={!['failed', 'cancelled', 'recovery_required'].includes(state)}><RotateCcw size={17} /> Retry</button><button className="danger-button" onClick={() => void command('cancel')} disabled={!['navigating', 'paused', 'arrived'].includes(state)}><Square size={16} /> Cancel</button></div>{notice && <p className="notice">{notice}</p>}</Card></div>
  </div>;
}

function RecoveryPane({ status }: { status: Status }) {
  const [notice, setNotice] = useState('');
  const request = async (command: 'retry' | 'cancel') => {
    try {
      await sendCommand(command);
      setNotice(command === 'retry' ? 'Retry requested. The controller will re-plan from the confirmed checkpoint.' : 'Cancellation requested.');
    } catch (error) {
      setNotice(error instanceof Error ? error.message : 'Recovery command failed.');
    }
  };
  return <div className="recovery-backdrop" role="dialog" aria-modal="true" aria-labelledby="recovery-title">
    <section className="recovery-pane">
      <div className="recovery-banner"><AlertTriangle size={25} /><div><p className="eyebrow">Safety hold active</p><h2 id="recovery-title">Operator recovery required</h2></div><StatePill value="recovery_required" /></div>
      <p className="recovery-copy">The controller has stopped motion. Review the cause and physical track condition before authorising a retry.</p>
      <div className="recovery-details">
        <div><span>Robot</span><strong>{status.robot_id}</strong></div>
        <div><span>Mission</span><strong>{status.mission?.mission || '—'}</strong></div>
        <div><span>Active route</span><strong>{status.mission?.current || '—'} <ChevronRight size={14} /> {status.mission?.goal || '—'}</strong></div>
        <div><span>Last confirmed QR</span><strong>{status.last_checkpoint || 'None'}</strong></div>
        <div><span>Line/controller state</span><strong>{pretty(status.telemetry?.state)}</strong></div>
        <div><span>Route heading</span><strong>{status.telemetry?.route_heading || 'Unknown'}</strong></div>
      </div>
      <div className="cause"><span>Recovery cause</span><p>{status.mission?.event || status.telemetry?.message || 'No detailed cause received.'}</p></div>
      <div className="recovery-actions"><button className="primary" onClick={() => void request('retry')}><RotateCcw size={17} /> Retry from confirmed checkpoint</button><button className="danger-button" onClick={() => void request('cancel')}><Square size={16} /> Cancel mission</button></div>
      {notice && <p className="notice">{notice}</p>}
    </section>
  </div>;
}

function App() {
  const [page, setPage] = useState<Page>('overview');
  const { status, map, connected } = useBridge();
  const navigation: { id: Page; label: string; icon: typeof Boxes }[] = [{ id: 'overview', label: 'Fleet Overview', icon: Boxes }, { id: 'warehouse', label: 'Live Warehouse', icon: MapIcon }, { id: 'tasks', label: 'Tasks', icon: ClipboardList }];
  return <div className="app-shell"><aside><div className="brand"><div className="brand-mark"><Route size={22} /></div><div><strong>TrackTag</strong><span>OPERATIONS</span></div></div><nav>{navigation.map((item) => { const Icon = item.icon; return <button key={item.id} className={page === item.id ? 'selected' : ''} onClick={() => setPage(item.id)}><Icon size={18} />{item.label}</button>; })}</nav><div className="sidebar-foot"><span className={connected ? 'dot live' : 'dot'} /> Local bridge {connected ? 'connected' : 'unavailable'}</div></aside><main>{page === 'overview' && <Overview status={status} connected={connected} />}{page === 'warehouse' && <Warehouse status={status} map={map} />}{page === 'tasks' && <Tasks status={status} map={map} />}</main>{missionState(status) === 'recovery_required' && status && <RecoveryPane status={status} />}</div>;
}

export default App;

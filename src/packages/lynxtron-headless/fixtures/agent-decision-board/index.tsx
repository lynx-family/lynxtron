import { root, useState } from '@lynx-js/react';
import './index.css';

const lanes = [
  ['Ingress', 'API edge', 'Auth drift'],
  ['Compute', 'Payments', 'CPU surge'],
  ['Data', 'Ledger', 'Write lag'],
  ['Comms', 'Status', 'Draft ready'],
];

const pendingTimeline = [
  ['04:12', 'Alpha owns live triage'],
  ['04:15', 'Beta waiting for approval'],
  ['04:18', 'Pager rotation synced'],
];

const armedTimeline = [
  ['04:12', 'Alpha handed over triage'],
  ['04:15', 'Beta promoted to execute'],
  ['04:18', 'Pager escalation widened'],
];

function App() {
  const [promoted, setPromoted] = useState(false);
  const snapshot = promoted
    ? {
        mode: 'execute',
        selected: 'Beta',
        queue: '5',
        risk: 'high',
        escalations: '2',
        primary: 'armed',
        route: 'Mitigation lane B',
        owner: 'SRE Victor',
        eta: '09 min',
        action: 'Beta promoted',
        actionDetail: 'Mitigation packet is armed and cross-region rollback is queued.',
        tone: 'after',
      }
    : {
        mode: 'triage',
        selected: 'Alpha',
        queue: '7',
        risk: 'medium',
        escalations: '1',
        primary: 'pending',
        route: 'Assessment lane A',
        owner: 'SRE Mina',
        eta: '14 min',
        action: 'Promote Beta',
        actionDetail: 'Promote the standby responder when the signal matrix confirms impact.',
        tone: 'before',
      };
  const timeline = promoted ? armedTimeline : pendingTimeline;

  return (
    <view className={`page ${promoted ? 'pageExecute' : ''}`}>
      <view className="header">
        <view className="headerTop">
          <text className="eyebrow">Incident Response Console</text>
          <text className="modePill">Mode {snapshot.mode}</text>
        </view>
        <text className="title">Mobile Ops Decision Board</text>
        <text className="subtitle">{snapshot.route}</text>
      </view>

      <view className="summary">
        <view className="summaryCell wide">
          <text className="label">Responder</text>
          <text className="value">Selected {snapshot.selected}</text>
          <text className="subvalue">{snapshot.owner}</text>
        </view>
        <view className="summaryCell">
          <text className="label">Load</text>
          <text className="value">Queue {snapshot.queue}</text>
          <text className="subvalue">ETA {snapshot.eta}</text>
        </view>
      </view>

      <view className="summary lowerSummary">
        <view className="summaryCell">
          <text className="label">Exposure</text>
          <text className={`value ${promoted ? 'dangerText' : 'warnText'}`}>
            Risk {snapshot.risk}
          </text>
          <text className="subvalue">Signal confidence 82</text>
        </view>
        <view className="summaryCell">
          <text className="label">Escalation</text>
          <text className="value">Escalations {snapshot.escalations}</text>
          <text className="subvalue">Command bridge live</text>
        </view>
      </view>

      <view className="matrix">
        <view className="sectionHeader">
          <text className="sectionTitle">Signal Matrix</text>
          <text className="sectionBadge">
            Primary action {snapshot.primary}
          </text>
        </view>
        {lanes.map(([lane, service, condition], index) => (
          <view className="matrixRow" key={lane}>
            <text className="lane">{lane}</text>
            <text className="service">{service}</text>
            <text className={promoted && index < 2 ? 'statusHot' : 'status'}>
              {promoted && index < 2 ? 'Active' : condition}
            </text>
          </view>
        ))}
      </view>

      <view className="timeline">
        <text className="sectionTitle">Decision Timeline</text>
        {timeline.map(([time, event]) => (
          <view className="timelineRow" key={time}>
            <text className="time">{time}</text>
            <text className="event">{event}</text>
          </view>
        ))}
      </view>

      <view
        className={`actionCard ${promoted ? 'actionCardArmed' : ''}`}
        bindtap={() => setPromoted(true)}
      >
        <view className="actionCopy">
          <text className="actionKicker">Recommended Action</text>
          <text className="actionTitle">{snapshot.action}</text>
          <text className="actionDetail">{snapshot.actionDetail}</text>
        </view>
        <view className={`actionRail ${promoted ? 'actionRailHot' : ''}`}>
          <text className="railText">{promoted ? 'ARM' : 'GO'}</text>
        </view>
      </view>
    </view>
  );
}

root.render(<App />);

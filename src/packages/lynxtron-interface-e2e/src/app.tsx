import { root, useState } from '@lynx-js/react';
import './styles.css';

const rows = Array.from({ length: 10 }, (_, index) => `Scroll row ${index + 1}`);
const nestedRows = Array.from({ length: 8 }, (_, index) => `Nested scroll row ${index + 1}`);
const initialKeyedItems = [
  { key: 'alpha', label: 'Key item alpha' },
  { key: 'bravo', label: 'Key item bravo' },
  { key: 'charlie', label: 'Key item charlie' },
];
const reorderedKeyedItems = [
  { key: 'charlie', label: 'Key item charlie' },
  { key: 'alpha', label: 'Key item alpha' },
  { key: 'delta', label: 'Key item delta' },
];

function StatusLine({ label, value }: { label: string; value: string }) {
  return (
    <view className="statusLine">
      <text className="statusLabel">{label}</text>
      <text className="statusValue">{value}</text>
    </view>
  );
}

function App() {
  const [activated, setActivated] = useState(false);
  const [formPrimed, setFormPrimed] = useState(false);
  const [scrollMarked, setScrollMarked] = useState(false);
  const [selectedScrollRow, setSelectedScrollRow] = useState(0);
  const [nestedParentSeen, setNestedParentSeen] = useState(false);
  const [nestedChildSeen, setNestedChildSeen] = useState(false);
  const [catchParentSeen, setCatchParentSeen] = useState(false);
  const [catchChildSeen, setCatchChildSeen] = useState(false);
  const [eventLog, setEventLog] = useState<string[]>([]);
  const [keyedReordered, setKeyedReordered] = useState(false);
  const [outerScrollCount, setOuterScrollCount] = useState(0);
  const [innerScrollCount, setInnerScrollCount] = useState(0);
  const [standaloneScrollCount, setStandaloneScrollCount] = useState(0);
  const [selectionChangeCount, setSelectionChangeCount] = useState(0);
  const [selectionTapCount, setSelectionTapCount] = useState(0);

  const tapCount = activated ? 1 : 0;
  const inputValue = formPrimed ? 'alpha-42' : '';
  const textareaValue = formPrimed ? 'line one\nline two' : '';
  const animationState = activated ? 'done' : 'idle';
  const scrollState = selectedScrollRow
    ? `row ${selectedScrollRow} tapped`
    : scrollMarked
      ? 'marked row 8'
      : 'initial index 3';
  const nestedState =
    nestedParentSeen && nestedChildSeen && catchChildSeen && !catchParentSeen
      ? 'bubble and catch ok'
      : 'parent child idle';
  const propagationState =
    eventLog.length > 0 ? eventLog.join('>') : 'event matrix idle';
  const keyedItems = keyedReordered ? reorderedKeyedItems : initialKeyedItems;
  const keyedState = keyedReordered ? 'keyed reorder charlie alpha delta' : 'keyed order alpha bravo charlie';
  const nestedScrollState = `nested scroll outer ${outerScrollCount} inner ${innerScrollCount}`;
  const standaloneScrollState = `standalone scroll ${standaloneScrollCount}`;
  const selectionState = `selection state changes ${selectionChangeCount} taps ${selectionTapCount}`;

  const addEventLog = (label: string) => {
    setEventLog((current) => [...current, label]);
  };

  return (
    <view className="page">
      <view className="hero">
        <view className="titleBlock">
          <text className="title">Lynx open interface E2E</text>
          <text className="subtitle">report: ready</text>
        </view>
        <view
          className={`tapTarget ${activated ? 'tapTargetActive' : ''}`}
          bindtap={() => setActivated(true)}
        >
          <text className="tapTitle">Run interaction smoke</text>
          <text className="tapBody">{activated ? 'tap result active' : 'tap result idle'}</text>
        </view>
      </view>

      <view className="summary">
        <StatusLine label="report: tap count" value={`${tapCount}`} />
        <StatusLine label="report: animation final" value={animationState} />
        <StatusLine label="report: form value" value={inputValue || 'empty'} />
        <StatusLine label="report: scroll state" value={scrollState} />
        <StatusLine label="report: nested state" value={nestedState} />
      </view>

      <view className="section interactionLab">
        <text className="sectionTitle">Interaction bug hunt lab</text>
        <view className="labRow">
          <view
            className="labEventOuter"
            capture-bindtap={() => addEventLog('outer-capture')}
            bindtap={() => addEventLog('outer-bind')}
          >
            <view className="labEventMiddle" bindtap={() => addEventLog('middle-bind')}>
              <view className="labEventInner" bindtap={() => addEventLog('inner-bind')}>
                <text className="nestedChildText">Event matrix inner action</text>
              </view>
            </view>
          </view>
          <view
            className={`labNested ${nestedParentSeen ? 'nestedParentSeen' : ''}`}
            bindtap={() => setNestedParentSeen(true)}
          >
            <view
              className={`labNestedChild ${nestedChildSeen ? 'nestedChildSeen' : ''}`}
              bindtap={() => setNestedChildSeen(true)}
            >
              <text className="nestedChildText">Nested child action</text>
            </view>
            <text className="nestedReport">
              {nestedParentSeen ? 'nested parent seen' : 'nested parent idle'}
            </text>
            <text className="nestedReport">
              {nestedChildSeen ? 'nested child seen' : 'nested child idle'}
            </text>
          </view>
        </view>
        <view className="labRow">
          <view
            className={`labCatch ${catchParentSeen ? 'nestedParentSeen' : ''}`}
            bindtap={() => setCatchParentSeen(true)}
          >
            <view
              className={`labCatchChild ${catchChildSeen ? 'nestedChildSeen' : ''}`}
              catchtap={() => setCatchChildSeen(true)}
            >
              <text className="nestedChildText">Catch child action</text>
            </view>
            <text className="nestedReport">
              {catchParentSeen ? 'catch parent seen' : 'catch parent idle'}
            </text>
            <text className="nestedReport">
              {catchChildSeen ? 'catch child seen' : 'catch child idle'}
            </text>
          </view>
          <view className="labActions">
            <view className="labKeyedRow">
              {keyedItems.map((item) => (
                <view className="labKeyedCell" key={item.key}>
                  <text className="keyedText">{item.label}</text>
                </view>
              ))}
            </view>
            <view className="smallButton labButton" bindtap={() => setKeyedReordered(true)}>
              <text className="smallButtonText">Reorder keyed items</text>
            </view>
            <view className="smallButton labButton" bindtap={() => setFormPrimed(true)}>
              <text className="smallButtonText">Prime form state</text>
            </view>
          </view>
        </view>
        <text className="formReport">event matrix result {propagationState}</text>
        <text className="formReport">{keyedState}</text>
      </view>

      <view className="section">
        <text className="sectionTitle">Basic view text boxes</text>
        <view className="boxRow">
          <view className="metricBox metricBlue">
            <text className="metricLabel">view box stable</text>
            <text className="metricValue">128x72</text>
          </view>
          <view className="metricBox metricGreen">
            <text className="metricLabel">text nested</text>
            <text className="metricValue">visible</text>
          </view>
          <view
            className={`nestedCatchParent ${catchParentSeen ? 'nestedParentSeen' : ''}`}
            bindtap={() => setCatchParentSeen(true)}
          >
            <text className="nestedReport">
              {catchParentSeen ? 'catch parent seen' : 'catch parent idle'}
            </text>
            <view
              className={`nestedCatchChild ${catchChildSeen ? 'nestedChildSeen' : ''}`}
              catchtap={() => setCatchChildSeen(true)}
            >
              <text className="nestedChildText">Catch child action</text>
              <text className="nestedReport">
                {catchChildSeen ? 'catch child seen' : 'catch child idle'}
              </text>
            </view>
          </view>
        </view>
      </view>

      <view className="section">
        <text className="sectionTitle">Nested event surface</text>
        <view
          className={`nestedParent ${nestedParentSeen ? 'nestedParentSeen' : ''}`}
          bindtap={() => setNestedParentSeen(true)}
        >
          <text className="nestedReport">
            {nestedParentSeen ? 'nested parent seen' : 'nested parent idle'}
          </text>
          <view
            className={`nestedChild ${nestedChildSeen ? 'nestedChildSeen' : ''}`}
            bindtap={() => setNestedChildSeen(true)}
          >
            <text className="nestedChildText">Nested child action</text>
            <text className="nestedReport">
              {nestedChildSeen ? 'nested child seen' : 'nested child idle'}
            </text>
          </view>
        </view>
      </view>

      <view className="section">
        <text className="sectionTitle">Form controls</text>
        <view className="formRow">
          <input
            className="inputBox"
            placeholder="input placeholder"
            value={inputValue}
            maxlength={24}
          />
          <textarea
            className="textareaBox"
            placeholder="textarea placeholder"
            value={textareaValue}
            maxlength={48}
            maxlines={2}
          />
        </view>
        <view className="smallButton" bindtap={() => setFormPrimed(true)}>
          <text className="smallButtonText">Prime form state</text>
        </view>
        <text className="formReport">
          {formPrimed ? 'form programmatic state alpha-42 line two' : 'form programmatic state pending'}
        </text>
      </view>

      <view className="section">
        <text className="sectionTitle">Scroll view</text>
        <scroll-view
          className="scrollBox"
          scroll-orientation="vertical"
          initial-scroll-to-index={3}
          enable-scroll={true}
          scroll-bar-enable={true}
        >
          {rows.map((row, index) => (
            <view
              className={`scrollRow ${
                selectedScrollRow === index + 1 || (index === 7 && scrollMarked)
                  ? 'scrollRowMarked'
                  : ''
              }`}
              key={row}
              bindtap={() => setSelectedScrollRow(index + 1)}
            >
              <text className="scrollText">{row}</text>
            </view>
          ))}
        </scroll-view>
        <view className="smallButton" bindtap={() => setScrollMarked(true)}>
          <text className="smallButtonText">Mark scroll row</text>
        </view>
        <text className="formReport">
          {selectedScrollRow
            ? `scroll public state row ${selectedScrollRow} tapped`
            : scrollMarked
              ? 'scroll public state row 8 marked'
              : 'scroll public state initial index 3'}
        </text>
      </view>

      <view className="section">
        <text className="sectionTitle">Nested scroll and selection conflict</text>
        <scroll-view
          className="nestedOuterScroll"
          scroll-orientation="vertical"
          enable-scroll={true}
          bindscroll={() => setOuterScrollCount((count) => count + 1)}
        >
          <view className="nestedScrollSpacer">
            <text className="nestedReport">outer scroll top spacer</text>
          </view>
          <scroll-view
            className="nestedInnerScroll"
            scroll-orientation="vertical"
            enable-scroll={true}
            bindscroll={() => setInnerScrollCount((count) => count + 1)}
          >
            {nestedRows.map((row) => (
              <view className="nestedScrollRow" key={row}>
                <text className="scrollText">{row}</text>
              </view>
            ))}
          </scroll-view>
          <text
            className="selectionText"
            text-selection={true}
            custom-text-selection={true}
            bindselectionchange={() => setSelectionChangeCount((count) => count + 1)}
            bindtap={() => setSelectionTapCount((count) => count + 1)}
          >
            Selectable conflict text drag target
          </text>
          <text
            className="selectionText"
            text-selection={true}
            bindselectionchange={() => setSelectionChangeCount((count) => count + 1)}
            bindtap={() => setSelectionTapCount((count) => count + 1)}
          >
            Nested plain selection drag target
          </text>
          <view className="nestedScrollSpacer">
            <text className="nestedReport">outer scroll bottom spacer</text>
          </view>
        </scroll-view>
        <text className="formReport">{nestedScrollState}</text>
        <scroll-view
          className="standaloneSelectionScroll"
          scroll-orientation="vertical"
          enable-scroll={true}
          bindscroll={() => setStandaloneScrollCount((count) => count + 1)}
        >
          <view className="nestedScrollSpacer">
            <text className="nestedReport">standalone scroll top spacer</text>
          </view>
          <text
            className="selectionText"
            text-selection={true}
            bindselectionchange={() => setSelectionChangeCount((count) => count + 1)}
            bindtap={() => setSelectionTapCount((count) => count + 1)}
          >
            Standalone scroll selection drag target
          </text>
          <view className="nestedScrollSpacer">
            <text className="nestedReport">standalone scroll bottom spacer</text>
          </view>
        </scroll-view>
        <text className="formReport">{standaloneScrollState}</text>
        <text className="formReport">{selectionState}</text>
      </view>

      <view className="section cssGallery">
        <text className="sectionTitle">CSS effect gallery</text>
        <view className="galleryRow">
          <view className="effectCard effectBorder">
            <text className="effectText">border radius</text>
          </view>
          <view className="effectCard effectOpacity">
            <text className="effectText">opacity</text>
          </view>
          <view className={`motionBox ${activated ? 'motionBoxDone' : ''}`}>
            <text className="effectText">{activated ? 'motion final done' : 'motion final idle'}</text>
          </view>
        </view>
      </view>
    </view>
  );
}

root.render(<App />);

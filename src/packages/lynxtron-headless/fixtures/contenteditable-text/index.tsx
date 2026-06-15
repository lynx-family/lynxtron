import { root, useState } from '@lynx-js/react';
import './index.css';

type SelectionRange = {
  start: number;
  end: number;
};

type EventLogEntry = {
  id: number;
  name: string;
  inputType: string;
  data: string;
  ranges: string;
  selection: string;
  handled: boolean;
};

type TextToken = {
  id: number;
  kind: 'text';
  text: string;
  styled?: boolean;
};

type InlineImageToken = {
  id: number;
  kind: 'image';
};

type InlineViewToken = {
  id: number;
  kind: 'view';
  label: string;
};

type Token = TextToken | InlineImageToken | InlineViewToken;

type EditorModel = {
  nextId: number;
  tokens: Token[];
};

type HistorySnapshot = {
  model: EditorModel;
  selection: SelectionRange;
};

const editableTextProps = { contenteditable: 'true' } as any;
const editorElementId = 'pm-editor';
const inlineImageSrc =
  'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAACXBIWXMAAAsTAAALEwEAmpwYAAABW0lEQVR4nJ2STUsCURSGB6RsFWVjtQvCPpBaBVFUi6BV9kGQEImaEUWFSh+LFnFv2qTjjNJvqIWLVv2X6mcURCubc3rjioIgKDMvXLiL8z73nPNeTeug5CMGYhL9mlfFLOzsF7HtGRA3uXpi48WTee4YPUcWfV6V+Tsq0esasPLAZqzEyJQZyRI/uTKP55zIvEHOeoERr0OI90zKdHdG4dMFZXVJvxN5xsI9I1L4Q8JiZMvMKZOf28YJpeEfvsWsLuhCF/ShS0bzTOYIiwZjo8g4KDGuK4ybCv2c2vyasLFbjziUhn9QYqb+sqC3VkBQMqbzhCWDsVVkpCzGmU1fhxZXVbztfyQKX1BQWhdUa0JG7hjhPGHZIGe16Jgqna6rCEhnrRUyKqg2lXM2XSURVJ00AEOCzl2ZtcY4uqR3tRd1dw/QNE2XdKk68WRWUvEGJMKeAWMSfSrmTkX/vI7WtLNB7GAAAAAASUVORK5CYII=';

const initialModel: EditorModel = {
  nextId: 3,
  tokens: [
    {
      id: 1,
      kind: 'text',
      text: 'Editable text baseline demo. Select part of this sentence or place the caret, then use the buttons.',
    },
  ],
};

function cloneToken(token: Token): Token {
  return { ...token };
}

function cloneModel(model: EditorModel): EditorModel {
  return {
    nextId: model.nextId,
    tokens: model.tokens.map(cloneToken),
  };
}

function cloneSelection(selection: SelectionRange): SelectionRange {
  return {
    start: selection.start,
    end: selection.end,
  };
}

function createSnapshot(model: EditorModel, selection: SelectionRange): HistorySnapshot {
  return {
    model: cloneModel(model),
    selection: cloneSelection(selection),
  };
}

function tokenLength(token: Token) {
  return token.kind === 'text' ? token.text.length : 1;
}

function getStreamLength(tokens: Token[]) {
  return tokens.reduce((length, token) => length + tokenLength(token), 0);
}

function clampOffset(value: number, max: number) {
  return Math.max(0, Math.min(value, max));
}

function normalizeSelection(selection: SelectionRange, tokens: Token[]) {
  const max = getStreamLength(tokens);
  const start = clampOffset(Math.min(selection.start, selection.end), max);
  const end = clampOffset(Math.max(selection.start, selection.end), max);
  return { start, end };
}

function compactTextTokens(tokens: Token[]) {
  const compacted: Token[] = [];
  tokens.forEach((token) => {
    const previous = compacted[compacted.length - 1];
    if (
      previous?.kind === 'text' &&
      token.kind === 'text' &&
      Boolean(previous.styled) === Boolean(token.styled)
    ) {
      previous.text += token.text;
      return;
    }
    if (token.kind !== 'text' || token.text.length > 0) {
      compacted.push({ ...token });
    }
  });
  return compacted;
}

function insertTokenAt(tokens: Token[], offset: number, insertedToken: Token) {
  const result: Token[] = [];
  let cursor = 0;
  let inserted = false;

  tokens.forEach((token) => {
    const length = tokenLength(token);
    const nextCursor = cursor + length;

    if (!inserted && offset <= nextCursor) {
      if (token.kind === 'text') {
        const cut = clampOffset(offset - cursor, token.text.length);
        result.push({ ...token, text: token.text.slice(0, cut) });
        result.push(insertedToken);
        result.push({ ...token, text: token.text.slice(cut) });
      } else if (offset <= cursor) {
        result.push(insertedToken, token);
      } else {
        result.push(token, insertedToken);
      }
      inserted = true;
    } else {
      result.push(token);
    }

    cursor = nextCursor;
  });

  if (!inserted) {
    result.push(insertedToken);
  }

  return compactTextTokens(result);
}

function removeRange(tokens: Token[], range: SelectionRange) {
  const result: Token[] = [];
  let cursor = 0;

  tokens.forEach((token) => {
    const length = tokenLength(token);
    const nextCursor = cursor + length;

    if (range.end <= cursor || range.start >= nextCursor) {
      result.push(token);
    } else if (token.kind === 'text') {
      const localStart = clampOffset(range.start - cursor, token.text.length);
      const localEnd = clampOffset(range.end - cursor, token.text.length);
      result.push({ ...token, text: token.text.slice(0, localStart) });
      result.push({ ...token, text: token.text.slice(localEnd) });
    }

    cursor = nextCursor;
  });

  return compactTextTokens(result);
}

function findTextStyleAtOffset(tokens: Token[], offset: number) {
  let cursor = 0;
  for (const token of tokens) {
    const length = tokenLength(token);
    const nextCursor = cursor + length;
    if (token.kind === 'text' && offset >= cursor && offset <= nextCursor) {
      return token.styled;
    }
    cursor = nextCursor;
  }
  return undefined;
}

function replaceRangeWithText(tokens: Token[], range: SelectionRange, text: string, id: number) {
  if (text.length === 0) {
    return removeRange(tokens, range);
  }
  const styled = findTextStyleAtOffset(tokens, range.start);
  const token: TextToken = {
    id,
    kind: 'text',
    text,
    styled,
  };
  return insertTokenAt(removeRange(tokens, range), range.start, token);
}

function replaceRangeWithToken(tokens: Token[], range: SelectionRange, insertedToken: Token) {
  return insertTokenAt(removeRange(tokens, range), range.start, insertedToken);
}

function styleSelectedText(tokens: Token[], range: SelectionRange, nextId: number) {
  const result: Token[] = [];
  let cursor = 0;
  let idCursor = nextId;

  const pushText = (text: string, styled: boolean | undefined) => {
    if (text.length === 0) {
      return;
    }
    result.push({
      id: idCursor,
      kind: 'text',
      styled,
      text,
    });
    idCursor += 1;
  };

  tokens.forEach((token) => {
    const length = tokenLength(token);
    const nextCursor = cursor + length;

    if (token.kind !== 'text' || range.end <= cursor || range.start >= nextCursor) {
      result.push(token);
    } else {
      const localStart = clampOffset(range.start - cursor, token.text.length);
      const localEnd = clampOffset(range.end - cursor, token.text.length);
      pushText(token.text.slice(0, localStart), token.styled);
      pushText(token.text.slice(localStart, localEnd), true);
      pushText(token.text.slice(localEnd), token.styled);
    }

    cursor = nextCursor;
  });

  return {
    nextId: idCursor,
    tokens: compactTextTokens(result),
  };
}

function getRangeFromDetail(detail: any, fallback: SelectionRange, tokens: Token[]) {
  const targetRanges =
    typeof detail?.getTargetRanges === 'function'
      ? detail.getTargetRanges()
      : detail?.targetRanges;
  const range = Array.isArray(targetRanges) ? targetRanges[0] : null;
  if (range) {
    const start = Number(range.startOffset ?? range.start);
    const end = Number(range.endOffset ?? range.end);
    if (Number.isFinite(start) && Number.isFinite(end)) {
      return normalizeSelection({ start, end }, tokens);
    }
  }

  const selection = detail?.selection;
  if (selection) {
    const start = Number(selection.startOffset ?? selection.anchorOffset);
    const end = Number(selection.endOffset ?? selection.focusOffset);
    if (Number.isFinite(start) && Number.isFinite(end)) {
      return normalizeSelection({ start, end }, tokens);
    }
  }

  return normalizeSelection(fallback, tokens);
}

function getRangeLabel(event: any) {
  const detail = event?.detail ?? {};
  const targetRanges =
    typeof detail.getTargetRanges === 'function'
      ? detail.getTargetRanges()
      : detail.targetRanges;
  if (!Array.isArray(targetRanges)) {
    return '';
  }
  return targetRanges
    .map((range) => `${range.startOffset ?? range.start}-${range.endOffset ?? range.end}`)
    .join(',');
}

function getSelectionLabel(event: any, selection: SelectionRange) {
  const detail = event?.detail ?? {};
  if (typeof detail.selection?.startOffset === 'number') {
    return `${detail.selection.startOffset}-${detail.selection.endOffset}`;
  }
  return `${selection.start}-${selection.end}`;
}

function createDeletionRange(inputType: string, range: SelectionRange, tokens: Token[]) {
  if (range.start !== range.end) {
    return range;
  }
  const max = getStreamLength(tokens);
  if (inputType === 'deleteContentForward' || inputType === 'deleteForward') {
    return { start: range.start, end: clampOffset(range.start + 1, max) };
  }
  return { start: clampOffset(range.start - 1, max), end: range.start };
}

function syncEditableSelection(range: SelectionRange) {
  setTimeout(() => {
    lynx
      .createSelectorQuery()
      .select(`#${editorElementId}`)
      .invoke({
        method: 'setEditableSelectionRange',
        params: {
          start: range.start,
          end: range.end,
        },
      } as any)
      .exec();
  }, 16);
}

function App() {
  const [model, setModel] = useState<EditorModel>(initialModel);
  const [selection, setSelection] = useState<SelectionRange>({
    start: getStreamLength(initialModel.tokens),
    end: getStreamLength(initialModel.tokens),
  });
  const [commandSelection, setCommandSelection] = useState<SelectionRange | null>(null);
  const [undoStack, setUndoStack] = useState<HistorySnapshot[]>([]);
  const [redoStack, setRedoStack] = useState<HistorySnapshot[]>([]);
  const [eventLog, setEventLog] = useState<EventLogEntry[]>([]);

  const pushEventLog = (name: string, event: any, handled = false) => {
    const detail = event?.detail ?? {};
    const nextEntry: EventLogEntry = {
      id: Date.now(),
      name,
      inputType: String(detail.inputType ?? ''),
      data: String(detail.data ?? ''),
      ranges: getRangeLabel(event),
      selection: getSelectionLabel(event, selection),
      handled,
    };
    setEventLog((items) => [nextEntry, ...items].slice(0, 6));
  };

  const setActiveSelection = (range: SelectionRange) => {
    const normalized = normalizeSelection(range, model.tokens);
    setSelection(normalized);
    if (normalized.start !== normalized.end) {
      setCommandSelection(normalized);
    } else {
      setCommandSelection(null);
    }
  };

  const commitModel = (nextModel: EditorModel, nextSelection: SelectionRange) => {
    const normalizedSelection = normalizeSelection(nextSelection, nextModel.tokens);
    setUndoStack((items) => [
      ...items.slice(Math.max(items.length - 49, 0)),
      createSnapshot(model, selection),
    ]);
    setRedoStack([]);
    setModel(cloneModel(nextModel));
    setSelection(normalizedSelection);
    setCommandSelection(normalizedSelection.start === normalizedSelection.end ? null : normalizedSelection);
    syncEditableSelection(normalizedSelection);
  };

  const applyUndo = () => {
    const previous = undoStack[undoStack.length - 1];
    if (!previous) {
      return;
    }
    setRedoStack((items) => [
      ...items.slice(Math.max(items.length - 49, 0)),
      createSnapshot(model, selection),
    ]);
    setUndoStack(undoStack.slice(0, undoStack.length - 1));
    setModel(cloneModel(previous.model));
    setSelection(cloneSelection(previous.selection));
    setCommandSelection(previous.selection.start === previous.selection.end ? null : cloneSelection(previous.selection));
    syncEditableSelection(previous.selection);
  };

  const applyRedo = () => {
    const next = redoStack[redoStack.length - 1];
    if (!next) {
      return;
    }
    setUndoStack((items) => [
      ...items.slice(Math.max(items.length - 49, 0)),
      createSnapshot(model, selection),
    ]);
    setRedoStack(redoStack.slice(0, redoStack.length - 1));
    setModel(cloneModel(next.model));
    setSelection(cloneSelection(next.selection));
    setCommandSelection(next.selection.start === next.selection.end ? null : cloneSelection(next.selection));
    syncEditableSelection(next.selection);
  };

  const getCommandRange = () => {
    const range = normalizeSelection(selection, model.tokens);
    if (range.start !== range.end) {
      return range;
    }
    return commandSelection ? normalizeSelection(commandSelection, model.tokens) : range;
  };

  const insertInlineToken = (kind: 'image' | 'view') => {
    const range = getCommandRange();
    const token: Token =
      kind === 'image'
        ? { id: model.nextId, kind: 'image' }
        : { id: model.nextId, kind: 'view', label: 'INLINE VIEW' };
    const tokens = replaceRangeWithToken(model.tokens, range, token);
    commitModel(
      {
        nextId: model.nextId + 1,
        tokens,
      },
      { start: range.start + 1, end: range.start + 1 }
    );
  };

  const applyStyledSpan = () => {
    const range = getCommandRange();
    if (range.start === range.end) {
      return;
    }

    const styled = styleSelectedText(model.tokens, range, model.nextId);
    commitModel(
      {
        nextId: styled.nextId,
        tokens: styled.tokens,
      },
      range
    );
  };

  const handleSelectionChange = (event: any) => {
    const detail = event?.detail;
    if (typeof detail?.start === 'number' && typeof detail?.end === 'number') {
      setActiveSelection({ start: detail.start, end: detail.end });
    }
  };

  const handleBeforeInput = (event: any) => {
    const detail = event?.detail ?? {};
    const inputType = String(detail.inputType ?? '');
    const data = typeof detail.data === 'string' ? detail.data : '';
    const range = getRangeFromDetail(detail, selection, model.tokens);

    if (inputType === 'historyUndo') {
      applyUndo();
      pushEventLog('beforeinput', event, true);
      return;
    }
    if (inputType === 'historyRedo') {
      applyRedo();
      pushEventLog('beforeinput', event, true);
      return;
    }

    if (
      inputType === 'insertText' ||
      inputType === 'insertCompositionText' ||
      inputType === 'insertFromPaste'
    ) {
      if (data.length === 0) {
        pushEventLog('beforeinput', event, false);
        return;
      }
      const tokens = replaceRangeWithText(model.tokens, range, data, model.nextId);
      commitModel(
        {
          nextId: model.nextId + 1,
          tokens,
        },
        { start: range.start + data.length, end: range.start + data.length }
      );
      pushEventLog('beforeinput', event, true);
      return;
    }

    if (inputType.startsWith('delete')) {
      const deletionRange = createDeletionRange(inputType, range, model.tokens);
      const tokens = removeRange(model.tokens, deletionRange);
      commitModel(
        {
          nextId: model.nextId,
          tokens,
        },
        { start: deletionRange.start, end: deletionRange.start }
      );
      pushEventLog('beforeinput', event, true);
      return;
    }

    pushEventLog('beforeinput', event, false);
  };

  const cancelNativeBeforeInput = (event: any) => {
    'main thread';
    event.preventDefault();
  };

  const handleInput = (event: any) => {
    pushEventLog('input', event);
  };

  return (
    <view className="page">
      <view className="header">
        <text className="title">Frontend PM Rich Text Demo</text>
      </view>

      <view className="toolbar">
        <text className="toolButton" bindtap={() => insertInlineToken('image')}>
          Inline image
        </text>
        <text className="toolButton" bindtap={() => insertInlineToken('view')}>
          Inline view
        </text>
        <text className="toolButton" bindtap={applyStyledSpan}>
          Span style
        </text>
        <text
          className={undoStack.length > 0 ? 'toolButton' : 'toolButton disabledToolButton'}
          bindtap={applyUndo}
        >
          Undo
        </text>
        <text
          className={redoStack.length > 0 ? 'toolButton' : 'toolButton disabledToolButton'}
          bindtap={applyRedo}
        >
          Redo
        </text>
      </view>

      <view className="debugPanel">
        <text className="debugText">
          {`selection ${selection.start}-${selection.end} undo=${undoStack.length} redo=${redoStack.length} tokens=${model.tokens.length}`}
        </text>
        {eventLog.map((entry) => (
          <text className="debugText" key={`event-${entry.id}`}>
            {`${entry.name} type=${entry.inputType} data=${entry.data} range=${entry.ranges} sel=${entry.selection} handled=${entry.handled ? 'yes' : 'no'}`}
          </text>
        ))}
      </view>

      <text
        id={editorElementId}
        className="editorText"
        {...editableTextProps}
        main-thread:bindbeforeinput={cancelNativeBeforeInput}
        bindbeforeinput={handleBeforeInput}
        bindinput={handleInput}
        bindselectionchange={handleSelectionChange}
      >
        {model.tokens.map((token, index) => {
          if (token.kind === 'text') {
            return (
              <inline-text
                className={token.styled ? 'styledSpan' : 'plainTextSegment'}
                key={`text-${token.id}-${index}`}
              >
                {token.text}
              </inline-text>
            );
          }
          if (token.kind === 'image') {
            return (
              <inline-image
                className="inlineImageToken"
                key={`image-${token.id}`}
                src={inlineImageSrc}
              />
            );
          }
          return (
            <inline-view className="inlineViewToken" key={`view-${token.id}`}>
              <text className="inlineViewLabel">{token.label}</text>
            </inline-view>
          );
        })}
      </text>
    </view>
  );
}

root.render(<App />);

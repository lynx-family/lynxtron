import { root, useState } from '@lynx-js/react';
import './index.css';

type SelectionRange = {
  start: number;
  end: number;
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

const editableTextProps = { contenteditable: 'true' } as any;
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

function App() {
  const [model, setModel] = useState<EditorModel>(initialModel);
  const [selection, setSelection] = useState<SelectionRange>({ start: 0, end: 0 });
  const [commandSelection, setCommandSelection] = useState<SelectionRange | null>(null);

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
    setModel({ nextId: model.nextId + 1, tokens });
    setSelection({ start: range.start + 1, end: range.start + 1 });
    setCommandSelection(null);
  };

  const applyStyledSpan = () => {
    const range = getCommandRange();
    if (range.start === range.end) {
      return;
    }

    const styled = styleSelectedText(model.tokens, range, model.nextId);
    setModel({
      nextId: styled.nextId,
      tokens: styled.tokens,
    });
    setSelection(range);
    setCommandSelection(range);
  };

  const handleSelectionChange = (event: any) => {
    const detail = event?.detail;
    if (typeof detail?.start === 'number' && typeof detail?.end === 'number') {
      const range = { start: detail.start, end: detail.end };
      setSelection(range);
      if (range.start !== range.end) {
        setCommandSelection(range);
      }
    }
  };

  return (
    <view className="page">
      <view className="header">
        <text className="title">Contenteditable Inline Lab</text>
      </view>

      <view className="toolbar">
        <text className="toolButton" bindtap={() => insertInlineToken('image')}>
          Insert inline image
        </text>
        <text className="toolButton" bindtap={() => insertInlineToken('view')}>
          Insert inline view
        </text>
        <text className="toolButton" bindtap={applyStyledSpan}>
          Style selection
        </text>
      </view>

      <text
        className="editorText"
        {...editableTextProps}
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

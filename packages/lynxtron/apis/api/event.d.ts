export type Event<Params extends object = {}> = {
  preventDefault: () => void;
  readonly defaultPrevented: boolean;
} & Params;

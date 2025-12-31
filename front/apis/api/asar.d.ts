// Docs: electron_common_asar binding

export interface SplitPathResultNotAsar {
  /**
   * Whether the input path is an asar path.
   */
  isAsar: false;
}

export interface SplitPathResultAsar {
  /**
   * Whether the input path is an asar path.
   */
  isAsar: true;
  /**
   * The absolute path to the `.asar` archive.
   */
  asarPath: string;
  /**
   * The path inside the `.asar` archive.
   */
  filePath: string;
}

export type SplitPathResult = SplitPathResultNotAsar | SplitPathResultAsar;

export declare function splitPath(path: string | Buffer | URL): SplitPathResult;

export interface AsarFileIntegrity {
  /**
   * The hash algorithm.
   */
  algorithm: 'SHA256';
  /**
   * The expected hex digest.
   */
  hash: string;
}

export interface AsarFileInfo {
  /**
   * The file size in bytes.
   */
  size: number;
  /**
   * Whether the file is stored unpacked.
   */
  unpacked: boolean;
  /**
   * The byte offset within the archive.
   */
  offset: number;
  /**
   * Optional integrity information.
   */
  integrity?: AsarFileIntegrity;
}

export interface AsarFileStat {
  /**
   * The file size in bytes.
   */
  size: number;
  /**
   * The byte offset within the archive.
   */
  offset: number;
  /**
   * The file type code.
   */
  type: number;
}

export declare class Archive {
  /**
   * Create an archive handle from a `.asar` path.
   */
  constructor(asarPath: string);
  /**
   * Reads the offset and size of file.
   */
  getFileInfo(filePath: string): AsarFileInfo | false;
  /**
   * Returns a fake result of `fs.stat(path)`.
   */
  stat(filePath: string): AsarFileStat | false;
  /**
   * Returns all files under a directory.
   */
  readdir(dirPath: string): string[] | false;
  /**
   * Returns the path of file with symbol link resolved.
   */
  realpath(filePath: string): string | false;
  /**
   * Copy the file out into a temporary file and returns the new path.
   */
  copyFileOut(filePath: string): string | false;
  /**
   * Return the file descriptor.
   */
  getFdAndValidateIntegrityLater(): number;
}

declare global {
  namespace NodeJS {
    interface AsarArchive extends Archive {}
    interface AsarFileInfo {
      size: number;
      unpacked: boolean;
      offset: number;
      integrity?: AsarFileIntegrity;
    }
    interface AsarFileStat {
      size: number;
      offset: number;
      type: number;
    }
  }
}

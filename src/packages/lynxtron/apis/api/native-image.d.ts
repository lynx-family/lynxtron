// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { Size } from '../structures/size';
import { Rectangle } from '../structures/rectangle';

export interface CreateFromBitmapOptions {
  width: number;
  height: number;
  /**
   * Defaults to 1.0.
   */
  scaleFactor?: number;
}

export interface CreateFromBufferOptions {
  /**
   * Required for bitmap buffers.
   */
  width?: number;
  /**
   * Required for bitmap buffers.
   */
  height?: number;
  /**
   * Defaults to 1.0.
   */
  scaleFactor?: number;
}

export interface AddRepresentationOptions {
  /**
   * The scale factor to add the image representation for.
   */
  scaleFactor?: number;
  /**
   * Defaults to 0. Required if a bitmap buffer is specified as `buffer`.
   */
  width?: number;
  /**
   * Defaults to 0. Required if a bitmap buffer is specified as `buffer`.
   */
  height?: number;
  /**
   * The buffer containing the raw image data.
   */
  buffer?: Buffer;
  /**
   * The data URL containing either a base 64 encoded PNG or JPEG image.
   */
  dataURL?: string;
}

export interface BitmapOptions {
  /**
   * Defaults to 1.0.
   */
  scaleFactor?: number;
}

export interface ResizeOptions {
  /**
   * Defaults to the image's width.
   */
  width?: number;
  /**
   * Defaults to the image's height.
   */
  height?: number;
  /**
   * The desired quality of the resize image. Possible values include `good`,
   * `better`, or `best`. The default is `best`. These values express a desired
   * quality/speed tradeoff. They are translated into an algorithm-specific method
   * that depends on the capabilities (CPU, GPU) of the underlying platform. It is
   * possible for all three methods to be mapped to the same algorithm on a given
   * platform.
   */
  quality?: 'good' | 'better' | 'best';
}

export interface ToBitmapOptions {
  /**
   * Defaults to 1.0.
   */
  scaleFactor?: number;
}

export interface ToDataURLOptions {
  /**
   * Defaults to 1.0.
   */
  scaleFactor?: number;
}

export interface ToPNGOptions {
  /**
   * Defaults to 1.0.
   */
  scaleFactor?: number;
}

export declare class NativeImage {
  // Docs: https://electronjs.org/docs/api/native-image

  /**
   * Creates an empty `NativeImage` instance.
   */
  static createEmpty(): NativeImage;
  /**
   * Creates a new `NativeImage` instance from `buffer` that contains the raw bitmap
   * pixel data returned by `toBitmap()`. The specific format is platform-dependent.
   */
  static createFromBitmap(
    buffer: Buffer,
    options: CreateFromBitmapOptions
  ): NativeImage;
  /**
   * Creates a new `NativeImage` instance from `buffer`. Tries to decode as PNG or
   * JPEG first.
   */
  static createFromBuffer(
    buffer: Buffer,
    options?: CreateFromBufferOptions
  ): NativeImage;
  /**
   * Creates a new `NativeImage` instance from `dataUrl`, a base 64 encoded Data URL
   * string.
   */
  static createFromDataURL(dataURL: string): NativeImage;
  /**
   * Creates a new `NativeImage` instance from the `NSImage` that maps to the given
   * image name. See Apple's `NSImageName` documentation for a list of possible
   * values.
   *
   * The `hslShift` is applied to the image with the following rules:
   *
   * * `hsl_shift[0]` (hue): The absolute hue value for the image - 0 and 1 map to 0
   * and 360 on the hue color wheel (red).
   * * `hsl_shift[1]` (saturation): A saturation shift for the image, with the
   * following key values: 0 = remove all color. 0.5 = leave unchanged. 1 = fully
   * saturate the image.
   * * `hsl_shift[2]` (lightness): A lightness shift for the image, with the
   * following key values: 0 = remove all lightness (make all pixels black). 0.5 =
   * leave unchanged. 1 = full lightness (make all pixels white).
   *
   * This means that `[-1, 0, 1]` will make the image completely white and `[-1, 1,0]`
   * will make the image completely black.
   *
   * In some cases, the `NSImageName` doesn't match its string representation; one
   * example of this is `NSFolderImageName`, whose string representation would
   * actually be `NSFolder`. Therefore, you'll need to determine the correct string
   * representation for your image before passing it in. This can be done with the
   * following:
   *
   * where `SYSTEM_IMAGE_NAME` should be replaced with any value from this list.
   *
   * @platform darwin
   */
  static createFromNamedImage(
    imageName: string,
    hslShift?: number[]
  ): NativeImage;
  /**
   * Creates a new `NativeImage` instance from an image file (e.g., PNG or JPEG)
   * located at `path`. This method returns an empty image if the `path` does not
   * exist, cannot be read, or is not a valid image.
   */
  static createFromPath(path: string): NativeImage;
  /**
   * fulfilled with the file's thumbnail preview image, which is a NativeImage.
   *
   * Windows implementation will ignore `size.height` and scale the height
   * according to `size.width`.
   *
   * @platform darwin,win32
   */
  static createThumbnailFromPath(
    path: string,
    size: Size
  ): Promise<NativeImage>;
  /**
   * Add an image representation for a specific scale factor. This can be used to
   * programmatically add different scale factor representations to an image. This
   * can be called on empty images.
   */
  addRepresentation(options: AddRepresentationOptions): void;
  /**
   * The cropped image.
   */
  crop(rect: Rectangle): NativeImage;
  /**
   * The image's aspect ratio (width divided by height).
   *
   * If `scaleFactor` is passed, this will return the aspect ratio corresponding to
   * the image representation most closely matching the passed value.
   */
  getAspectRatio(scaleFactor?: number): number;
  /**
   * Legacy alias for `image.toBitmap()`.
   *
   * @deprecated Use `image.toBitmap()` instead.
   */
  getBitmap(options?: BitmapOptions): void;
  /**
   * A Buffer that stores C pointer to underlying native handle of the image. On
   * macOS, a pointer to `NSImage` instance is returned.
   *
   * Notice that the returned pointer is a weak pointer to the underlying native
   * image instead of a copy, so you _must_ ensure that the associated `nativeImage`
   * instance is kept around.
   *
   * @platform darwin
   */
  getNativeHandle(): Buffer;
  /**
   * An array of all scale factors corresponding to representations for a given
   * `NativeImage`.
   */
  getScaleFactors(): number[];
  /**
   * If `scaleFactor` is passed, this will return the size corresponding to the image
   * representation most closely matching the passed value.
   */
  getSize(scaleFactor?: number): Size;
  /**
   * Whether the image is empty.
   */
  isEmpty(): boolean;
  /**
   * Whether the image is a macOS template image.
   */
  isTemplateImage(): boolean;
  /**
   * The resized image.
   *
   * If only the `height` or the `width` are specified then the current aspect ratio
   * will be preserved in the resized image.
   */
  resize(options: ResizeOptions): NativeImage;
  /**
   * Marks the image as a macOS template image.
   */
  setTemplateImage(option: boolean): void;
  /**
   * A Buffer that contains a copy of the image's raw bitmap pixel data.
   */
  toBitmap(options?: ToBitmapOptions): Buffer;
  /**
   * The Data URL of the image.
   */
  toDataURL(options?: ToDataURLOptions): string;
  /**
   * A Buffer that contains the image's `JPEG` encoded data.
   */
  toJPEG(quality: number): Buffer;
  /**
   * A Buffer that contains the image's `PNG` encoded data.
   */
  toPNG(options?: ToPNGOptions): Buffer;
  /**
   * A `boolean` property that determines whether the image is considered a template
   * image.
   *
   * Please note that this property only has an effect on macOS.
   *
   * @platform darwin
   */
  isMacTemplateImage: boolean;
}

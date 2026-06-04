#!/usr/bin/env python3

"""Generates localized InfoPlist.strings files for the macOS app bundle."""

from pathlib import Path
import argparse


FALLBACK_ENTRIES = {
    "CFBundleDisplayName": "{product_name}",
    "CFBundleName": "{product_name}",
    "CFBundleSpokenName": "{product_name}",
    "NSMicrophoneUsageDescription": "This app needs access to the microphone",
    "NSCameraUsageDescription": "This app needs access to the camera",
    "NSBluetoothAlwaysUsageDescription": "This app needs access to Bluetooth",
    "NSBluetoothPeripheralUsageDescription":
        "This app needs access to Bluetooth",
}

LOCALIZED_ENTRIES = {
    "zh_CN": {
        "NSMicrophoneUsageDescription": "此应用需要访问麦克风",
        "NSCameraUsageDescription": "此应用需要访问摄像头",
        "NSBluetoothAlwaysUsageDescription": "此应用需要访问蓝牙",
        "NSBluetoothPeripheralUsageDescription": "此应用需要访问蓝牙",
    },
}


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument("--product-name", required=True)
  parser.add_argument("outputs", nargs="+")
  return parser.parse_args()


def write_strings_file(output_path: Path, product_name: str) -> None:
  locale = output_path.parent.name.removesuffix(".lproj")
  entries = dict(FALLBACK_ENTRIES)
  entries.update(LOCALIZED_ENTRIES.get(locale, {}))

  lines = [
      f"\"{key}\" = \"{value.format(product_name=product_name)}\";"
      for key, value in entries.items()
  ]
  output_path.parent.mkdir(parents=True, exist_ok=True)
  output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
  options = parse_args()
  for output in options.outputs:
    write_strings_file(Path(output), options.product_name)


if __name__ == "__main__":
  main()

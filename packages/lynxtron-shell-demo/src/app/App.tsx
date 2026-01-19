import { useCallback, useEffect, useState } from '@lynx-js/react'

import './App.css'
import placeholder from '@assets/placeholder.png'

export function App(props: {
  onRender?: () => void
}) {
  return (
    <view className="Background">
      <image className="BackgroundImage" src={placeholder}></image>
      <text className="Content">
          Hello, Lynxtron~ dsffdas  asff
      </text>
    </view>
  )
}

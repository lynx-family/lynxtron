// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import React from 'react';

interface CardProps {
  title: string;
  description: string;
  icon: string;
}

const Card: React.FC<CardProps> = ({ title, description, icon }) => {
  return (
    <view
      style={{
        backgroundColor: '#171717',
        borderRadius: '16px',
        border: '1px solid #2b2b2b',
        padding: '32px',
        display: 'flex',
        flexDirection: 'column',
        gap: '16px',
        width: '350px',
        minHeight: '200px',
      }}
    >
      <image
        src={icon}
        mode="aspectFit"
        style={{ width: '32px', height: '32px' }}
      />
      <text
        style={{
          color: '#FFFFFF',
          fontSize: '20px',
          fontWeight: '700',
          margin: 0,
          overflowWrap: 'break-word',
        }}
      >
        {title}
      </text>
      <text
        style={{
          color: 'rgba(255, 255, 255, 0.65)',
          fontSize: '14px',
          margin: 0,
          overflowWrap: 'break-word',
        }}
      >
        {description}
      </text>
    </view>
  );
};

export default Card;

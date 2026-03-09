import React from 'react';
import Card from './Card';
import lightning from '../assets/lightning.png';
import box from '../assets/box.png';
import panel from '../assets/panel.png';

const FeatureGrid: React.FC = () => {
  const features = [
    {
      title: 'Light-weight and Fast',
      description:
        'Electron like app framework with A Light-weight UI Renderer Powered by Lynx.',
      icon: lightning,
    },
    {
      title: 'Natively Extensible',
      description:
        'Extend the renderer’s capabilities with custom native modules via UI/texture extension C-APIs and Node-API',
      icon: box,
    },
    {
      title: 'Multiplatform',
      description:
        'With Lynx, your UI runs across platforms—including the Web—and can be ported to new hosts with minimal effort',
      icon: panel,
    },
  ];

  return (
    <view
      style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 1fr)',
        gap: '20px',
        padding: '20px',
        boxSizing: 'border-box',
      }}
    >
      {features.map((feature, index) => (
        <Card key={index} {...feature} />
      ))}
    </view>
  );
};

export default FeatureGrid;

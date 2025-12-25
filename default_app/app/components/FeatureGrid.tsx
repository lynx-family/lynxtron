import React from 'react';
import Card from './Card';
import lightning from '../assets/lightning.png';
import box from '../assets/box.png';
import panel from '../assets/panel.png';

const FeatureGrid: React.FC = () => {
  const features = [
    {
      title: 'High-Performance Inter-App Communications',
      description:
        'Universal plug-in mechanism supports cross-process access to Node.js, Rust, C++ and other business logic, reducing the load of the main process and improving flexibility.',
      icon: lightning,
    },
    {
      title: 'Native-Extensible Architecture',
      description:
        'Leverages Lynx’s native extension points, allowing developers to enhance rendering capabilities with custom extension components—tailoring the framework to unique project needs.',
      icon: box,
    },
    {
      title: 'Ultimate Performance',
      description:
        'Compared to Electron, Lynxtron offers a smaller package size and extreme performance: faster startup and lower runtime memory usage.',
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

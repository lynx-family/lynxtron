import { root, useState } from '@lynx-js/react';
import './index.css';

const commerceStandardMock = {
  user: {
    id: 'user-lynx-001',
    name: 'Mina Chen',
    tier: 'Gold member',
    city: 'Shanghai',
    points: 4820,
    coupons: 6,
  },
  tabs: [
    { id: 'recommend', label: 'Recommend' },
    { id: 'profile', label: 'Profile' },
  ],
  home: {
    title: 'Lynx Market',
    subtitle: 'Daily Picks',
    cartLabel: 'Cart 2',
    chips: ['All', 'Home', 'Wear', 'Tech'],
  },
  products: [
    {
      id: 'p-nova-lamp',
      name: 'Nova Desk Lamp',
      price: '$48',
      rating: '4.8',
      sold: '1.2k sold',
      badge: 'Top Pick',
      detailTitle: 'Detail Nova Desk Lamp',
      detailSubtitle: 'Warm dimmable light for compact workspaces.',
      inventory: 'Inventory 24',
      shipping: 'Delivery tomorrow',
      option: 'Color Sage',
    },
    {
      id: 'p-orbit-jacket',
      name: 'Orbit Knit Jacket',
      price: '$72',
      rating: '4.7',
      sold: '860 sold',
      badge: 'New',
      detailTitle: 'Detail Orbit Knit Jacket',
      detailSubtitle: 'Soft layered knit with a weather resistant shell.',
      inventory: 'Inventory 18',
      shipping: 'Delivery Friday',
      option: 'Size M',
    },
    {
      id: 'p-canvas-tote',
      name: 'Canvas Mini Tote',
      price: '$36',
      rating: '4.9',
      sold: '2.1k sold',
      badge: 'Hot',
      detailTitle: 'Detail Canvas Mini Tote',
      detailSubtitle: 'Structured cotton tote with an inner zipper pocket.',
      inventory: 'Inventory 42',
      shipping: 'Delivery tomorrow',
      option: 'Color Oat',
    },
    {
      id: 'p-pulse-speaker',
      name: 'Pulse Travel Speaker',
      price: '$59',
      rating: '4.6',
      sold: '740 sold',
      badge: 'Deal',
      detailTitle: 'Detail Pulse Travel Speaker',
      detailSubtitle: 'Pocket speaker with splash protection and deep bass.',
      inventory: 'Inventory 31',
      shipping: 'Delivery Saturday',
      option: 'Color Ink',
    },
    {
      id: 'p-bloom-mug',
      name: 'Bloom Ceramic Mug',
      price: '$22',
      rating: '4.8',
      sold: '980 sold',
      badge: 'Gift',
      detailTitle: 'Detail Bloom Ceramic Mug',
      detailSubtitle: 'Hand glazed mug with a heat holding rounded body.',
      inventory: 'Inventory 55',
      shipping: 'Delivery tomorrow',
      option: 'Color Coral',
    },
    {
      id: 'p-zen-mat',
      name: 'Zen Desk Mat',
      price: '$31',
      rating: '4.7',
      sold: '670 sold',
      badge: 'Focus',
      detailTitle: 'Detail Zen Desk Mat',
      detailSubtitle: 'Low profile mat for keyboard, mouse, and notes.',
      inventory: 'Inventory 29',
      shipping: 'Delivery Thursday',
      option: 'Color Charcoal',
    },
    {
      id: 'p-terra-kit',
      name: 'Terra Pour Over Kit',
      price: '$64',
      rating: '4.9',
      sold: '410 sold',
      badge: 'Craft',
      detailTitle: 'Detail Terra Pour Over Kit',
      detailSubtitle: 'Ceramic dripper set with glass server and filters.',
      inventory: 'Inventory 16',
      shipping: 'Delivery Sunday',
      option: 'Set 02',
    },
    {
      id: 'p-metro-pack',
      name: 'Metro Sling Pack',
      price: '$54',
      rating: '4.5',
      sold: '530 sold',
      badge: 'Travel',
      detailTitle: 'Detail Metro Sling Pack',
      detailSubtitle: 'Compact crossbody pack with tablet-safe storage.',
      inventory: 'Inventory 21',
      shipping: 'Delivery Friday',
      option: 'Color Navy',
    },
    {
      id: 'p-aero-stand',
      name: 'Aero Laptop Stand',
      price: '$46',
      rating: '4.6',
      sold: '790 sold',
      badge: 'Desk',
      detailTitle: 'Detail Aero Laptop Stand',
      detailSubtitle: 'Foldable aluminum stand with six height positions.',
      inventory: 'Inventory 38',
      shipping: 'Delivery tomorrow',
      option: 'Finish Silver',
    },
    {
      id: 'p-luma-cable',
      name: 'Luma Braided Cable',
      price: '$18',
      rating: '4.7',
      sold: '3.4k sold',
      badge: 'Saver',
      detailTitle: 'Detail Luma Braided Cable',
      detailSubtitle: 'Fast charge braided cable with reinforced ends.',
      inventory: 'Inventory 120',
      shipping: 'Delivery tomorrow',
      option: 'Length 1m',
    },
  ],
  history: [
    { id: 'h-001', label: 'Viewed Nova Desk Lamp', time: 'Today 09:18' },
    { id: 'h-002', label: 'Viewed Canvas Mini Tote', time: 'Yesterday 21:04' },
    { id: 'h-003', label: 'Bought Luma Braided Cable', time: 'May 27 14:32' },
  ],
  settings: [
    { id: 's-address', label: 'Address Book', value: '2 saved' },
    { id: 's-payment', label: 'Payment Methods', value: 'Visa 2048' },
    { id: 's-notify', label: 'Notification Settings', value: 'Deals on' },
  ],
} as const;

type Product = (typeof commerceStandardMock.products)[number];
type Screen = 'recommend' | 'profile' | 'detail';

const productRows = [
  commerceStandardMock.products.slice(0, 2),
  commerceStandardMock.products.slice(2, 4),
  commerceStandardMock.products.slice(4, 6),
  commerceStandardMock.products.slice(6, 8),
  commerceStandardMock.products.slice(8, 10),
];

function HomeHeader() {
  return (
    <view className="homeHeader">
      <view className="topBar">
        <view className="brandStack">
          <text className="marketTitle">{commerceStandardMock.home.title}</text>
          <text className="marketSubtitle">{commerceStandardMock.home.subtitle}</text>
        </view>
        <text className="cartPill">{commerceStandardMock.home.cartLabel}</text>
      </view>
      <view className="chipRow">
        {commerceStandardMock.home.chips.map((chip, index) => (
          <text
            className={`chip ${index === 0 ? 'chipActive' : ''}`}
            key={chip}
          >
            {chip}
          </text>
        ))}
      </view>
    </view>
  );
}

function ProductCard({
  product,
  index,
  onOpen,
}: {
  product: Product;
  index: number;
  onOpen: (product: Product) => void;
}) {
  const isNova = product.id === 'p-nova-lamp';

  return (
    <view className="productCard" bindtap={() => onOpen(product)}>
      <view className={`productVisual tileTone${index % 5}`}>
        <text className="badge">{product.badge}</text>
      </view>
      <text className="productName">{product.name}</text>
      <view className="productMeta">
        <text className="price">{product.price}</text>
        <text className="rating">Rating {product.rating}</text>
      </view>
      <text className="sold">{product.sold}</text>
      <view className={isNova ? 'openButton primaryOpen' : 'openButton'}>
        <text className={isNova ? 'openButtonText primaryOpenText' : 'openButtonText'}>
          {isNova ? 'Open Nova Desk Lamp' : 'View Product'}
        </text>
      </view>
    </view>
  );
}

function HomePage({ onOpen }: { onOpen: (product: Product) => void }) {
  return (
    <view className="contentWithTabs">
      <HomeHeader />
      <scroll-view
        className="homeScroll"
        scroll-orientation="vertical"
        scroll-bar-enable={true}
        enable-scroll={true}
      >
        {productRows.map((row, rowIndex) => (
          <view className="productRow" key={`product-row-${rowIndex}`}>
            {row.map((product, index) => (
              <ProductCard
                product={product}
                index={rowIndex * 2 + index}
                key={product.id}
                onOpen={onOpen}
              />
            ))}
          </view>
        ))}
      </scroll-view>
    </view>
  );
}

function DetailPage({
  product,
  onBack,
}: {
  product: Product;
  onBack: () => void;
}) {
  return (
    <view className="detailPage">
      <view className="detailHeader">
        <text className="detailKicker">Product Detail</text>
        <text className="detailTitle">{product.detailTitle}</text>
      </view>

      <view className="detailHero">
        <view className="detailVisual">
          <text className="detailBadge">{product.badge}</text>
        </view>
        <view className="detailPriceBlock">
          <text className="detailPrice">{product.price}</text>
          <text className="detailRating">Rating {product.rating}</text>
          <text className="detailSold">{product.sold}</text>
        </view>
      </view>

      <view className="detailFacts">
        <text className="factPill">{product.inventory}</text>
        <text className="factPill">{product.shipping}</text>
        <text className="factPill">{product.option}</text>
      </view>

      <view className="descriptionPanel">
        <text className="sectionLabel">About this item</text>
        <text className="descriptionText">{product.detailSubtitle}</text>
      </view>

      <view className="detailAction" bindtap={() => {}}>
        <text className="detailActionText">
          {product.id === 'p-nova-lamp' ? 'Add Nova To Cart' : `Add ${product.name} To Cart`}
        </text>
      </view>

      <view className="backAction" bindtap={onBack}>
        <text className="backActionText">Back To Recommendations</text>
      </view>
    </view>
  );
}

function ProfilePage() {
  const user = commerceStandardMock.user;

  return (
    <view className="contentWithTabs">
      <view className="profilePage">
        <view className="profileHeader">
          <text className="profileTitle">Profile Center</text>
          <text className="profileName">{user.name}</text>
          <text className="profileTier">{user.tier}</text>
        </view>

        <view className="profileStats">
          <view className="statCell">
            <text className="statLabel">Points {user.points}</text>
            <text className="statSub">Rewards ready</text>
          </view>
          <view className="statCell statCellRight">
            <text className="statLabel">Coupons {user.coupons}</text>
            <text className="statSub">Valid this week</text>
          </view>
        </view>

        <view className="profileSection">
          <text className="profileSectionTitle">History</text>
          {commerceStandardMock.history.map((item) => (
            <view className="profileRow" key={item.id}>
              <text className="rowMain">{item.label}</text>
              <text className="rowSide">{item.time}</text>
            </view>
          ))}
        </view>

        <view className="profileSection settingsSection">
          <text className="profileSectionTitle">Settings</text>
          {commerceStandardMock.settings.map((item) => (
            <view className="profileRow" key={item.id}>
              <text className="rowMain">{item.label}</text>
              <text className="rowSide">{item.value}</text>
            </view>
          ))}
        </view>
      </view>
    </view>
  );
}

function TabBar({
  selected,
  onSelect,
}: {
  selected: Screen;
  onSelect: (screen: Screen) => void;
}) {
  return (
    <view className="tabBar">
      {commerceStandardMock.tabs.map((tab) => {
        const isSelected = selected === tab.id;
        return (
          <view
            className={`tabItem ${isSelected ? 'tabItemActive' : ''}`}
            key={tab.id}
            bindtap={() => onSelect(tab.id as Screen)}
          >
            <text className={isSelected ? 'tabGlyph tabGlyphActive' : 'tabGlyph'}>
              {tab.id === 'recommend' ? 'R' : 'P'}
            </text>
            <text className={isSelected ? 'tabLabel tabLabelActive' : 'tabLabel'}>
              {tab.label}
            </text>
          </view>
        );
      })}
    </view>
  );
}

function App() {
  const [screen, setScreen] = useState<Screen>('recommend');
  const [detailProduct, setDetailProduct] = useState<Product>(
    commerceStandardMock.products[0]
  );

  const openProduct = (product: Product) => {
    setDetailProduct(product);
    setScreen('detail');
  };

  const selectTab = (nextScreen: Screen) => {
    setScreen(nextScreen === 'profile' ? 'profile' : 'recommend');
  };

  return (
    <view className="app">
      {screen === 'detail' ? (
        <DetailPage product={detailProduct} onBack={() => setScreen('recommend')} />
      ) : (
        <>
          {screen === 'profile' ? (
            <ProfilePage />
          ) : (
            <HomePage onOpen={openProduct} />
          )}
          <TabBar selected={screen} onSelect={selectTab} />
        </>
      )}
    </view>
  );
}

root.render(<App />);

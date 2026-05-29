// Copyright 2026 The Lynxtron Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

import { useState } from '@lynx-js/react';

import { commerceStandardMock } from './mock';
import './styles.css';

type Product = (typeof commerceStandardMock.products)[number];
type TabId = (typeof commerceStandardMock.tabs)[number]['id'];
type Surface = 'recommend' | 'detail' | 'profile';

const novaProduct = commerceStandardMock.products[0];
const productRows = [
  commerceStandardMock.products.slice(0, 2),
  commerceStandardMock.products.slice(2, 4),
  commerceStandardMock.products.slice(4, 6),
  commerceStandardMock.products.slice(6, 8),
  commerceStandardMock.products.slice(8, 10),
];

function BottomTabs({
  activeTab,
  onSelect,
}: {
  activeTab: TabId;
  onSelect: (tab: TabId) => void;
}) {
  return (
    <view className="CommerceTabBar">
      {commerceStandardMock.tabs.map((tab, index) => {
        const selected = tab.id === activeTab;
        return (
          <view
            key={tab.id}
            className={`CommerceTabItem ${index === 0 ? 'CommerceTabItemLead' : ''} ${
              selected ? 'CommerceTabItemActive' : ''
            }`}
            bindtap={() => onSelect(tab.id)}
          >
            <text
              className={`CommerceTabText ${selected ? 'CommerceTabTextActive' : ''}`}
            >
              {tab.label}
            </text>
          </view>
        );
      })}
    </view>
  );
}

function CategoryChips() {
  return (
    <view className="CommerceChipRow">
      {commerceStandardMock.home.chips.map((chip, index) => (
        <text
          key={chip}
          className={`CommerceChip ${index === 0 ? 'CommerceChipActive' : ''}`}
        >
          {chip}
        </text>
      ))}
    </view>
  );
}

function ProductCard({
  product,
  index,
  isLead,
  onOpenNova,
}: {
  product: Product;
  index: number;
  isLead: boolean;
  onOpenNova: () => void;
}) {
  const isNova = product.id === 'p-nova-lamp';

  return (
    <view
      className={`CommerceProductCard ${isLead ? 'CommerceProductCardLead' : ''}`}
    >
      <view className={`CommerceProductArt CommerceProductArt${index % 5}`}>
        <text className="CommerceProductBadge">{product.badge}</text>
      </view>
      <text className="CommerceProductName">{product.name}</text>
      <view className="CommerceProductMeta">
        <text className="CommerceProductPrice">{product.price}</text>
        <text className="CommerceProductRating">Rating {product.rating}</text>
      </view>
      <text className="CommerceProductSold">{product.sold}</text>
      {isNova ? (
        <view className="CommerceOpenNova" bindtap={onOpenNova}>
          <text className="CommerceOpenNovaText">Open Nova Desk Lamp</text>
        </view>
      ) : (
        <view className="CommerceCardFoot">
          <text className="CommerceCardFootText">{product.badge}</text>
        </view>
      )}
    </view>
  );
}

function RecommendHome({ onOpenNova }: { onOpenNova: () => void }) {
  return (
    <view className="CommercePage">
      <view className="CommerceTop">
        <view className="CommerceHeaderRow">
          <view className="CommerceTitleBlock">
            <text className="CommerceTitle">{commerceStandardMock.home.title}</text>
            <text className="CommerceSubtitle">
              {commerceStandardMock.home.subtitle}
            </text>
          </view>
          <view className="CommerceCartBadge">
            <text className="CommerceCartText">
              {commerceStandardMock.home.cartLabel}
            </text>
          </view>
        </view>
        <CategoryChips />
      </view>

      <view className="CommerceListArea">
        <scroll-view
          className="CommerceProductList"
          scroll-orientation="vertical"
          scroll-bar-enable={true}
          enable-scroll={true}
        >
          {productRows.map((row, rowIndex) => (
            <view className="CommerceProductRow" key={`product-row-${rowIndex}`}>
              {row.map((product, index) => (
                <ProductCard
                  product={product}
                  index={rowIndex * 2 + index}
                  isLead={index === 0}
                  key={product.id}
                  onOpenNova={onOpenNova}
                />
              ))}
            </view>
          ))}
        </scroll-view>
      </view>
    </view>
  );
}

function ProductDetail({ onBack }: { onBack: () => void }) {
  return (
    <view className="CommercePage DetailPage">
      <view className="CommerceDetailTop">
        <view className="CommerceBackButton" bindtap={onBack}>
          <text className="CommerceBackText">Back To Recommendations</text>
        </view>
        <text className="CommerceDetailLabel">Product Detail</text>
      </view>

      <view className="CommerceDetailBody">
        <view className="CommerceDetailHero">
          <text className="CommerceDetailHeroBadge">{novaProduct.badge}</text>
        </view>

        <view className="CommerceDetailPanel">
          <text className="CommerceDetailTitle">{novaProduct.detailTitle}</text>
          <text className="CommerceDetailPrice">{novaProduct.price}</text>
          <view className="CommerceDetailInfoStack">
            <view className="CommerceDetailInfoRow">
              <view className="CommerceDetailInfo">
                <text className="CommerceDetailInfoText">
                  {novaProduct.inventory}
                </text>
              </view>
              <view className="CommerceDetailInfo CommerceDetailInfoRight">
                <text className="CommerceDetailInfoText">
                  {novaProduct.shipping}
                </text>
              </view>
            </view>
            <view className="CommerceDetailInfo CommerceDetailInfoWide">
              <text className="CommerceDetailInfoText">{novaProduct.option}</text>
            </view>
          </view>
          <text className="CommerceDetailCopy">{novaProduct.detailSubtitle}</text>
          <view className="CommerceAddAction">
            <text className="CommerceAddActionText">Add Nova To Cart</text>
          </view>
        </view>
      </view>
    </view>
  );
}

function ProfilePage() {
  return (
    <view className="CommercePage">
      <scroll-view
        className="CommerceProfileScroll"
        scroll-orientation="vertical"
        scroll-bar-enable={true}
        enable-scroll={true}
      >
        <view className="CommerceProfileBody">
          <view className="CommerceProfileHeader">
            <text className="CommerceProfileTitle">Profile Center</text>
            <text className="CommerceProfileName">
              {commerceStandardMock.user.name}
            </text>
            <text className="CommerceProfileTier">
              {commerceStandardMock.user.tier}
            </text>
          </view>

          <view className="CommerceStatsRow">
            <view className="CommerceStat">
              <text className="CommerceStatLabel">Points</text>
              <text className="CommerceStatValue">
                {`Points ${commerceStandardMock.user.points}`}
              </text>
            </view>
            <view className="CommerceStat CommerceStatRight">
              <text className="CommerceStatLabel">Coupons</text>
              <text className="CommerceStatValue">
                {`Coupons ${commerceStandardMock.user.coupons}`}
              </text>
            </view>
          </view>

          <text className="CommerceSectionTitle">History</text>
          <view className="CommerceRows">
            {commerceStandardMock.history.map((row) => (
              <view key={row.id} className="CommerceDataRow">
                <text className="CommerceDataLabel">{row.label}</text>
                <text className="CommerceDataValue">{row.time}</text>
              </view>
            ))}
          </view>

          <text className="CommerceSectionTitle CommerceSettingsTitle">
            Settings
          </text>
          <view className="CommerceRows">
            {commerceStandardMock.settings.map((row) => (
              <view key={row.id} className="CommerceDataRow">
                <text className="CommerceDataLabel">{row.label}</text>
                <text className="CommerceDataValue">{row.value}</text>
              </view>
            ))}
          </view>
        </view>
      </scroll-view>
    </view>
  );
}

export function CommerceDevtoolAgent() {
  const [surface, setSurface] = useState<Surface>('recommend');

  const handleTabSelect = (tab: TabId) => {
    setSurface(tab === 'profile' ? 'profile' : 'recommend');
  };

  if (surface === 'detail') {
    return (
      <view className="CommerceShell">
        <ProductDetail onBack={() => setSurface('recommend')} />
      </view>
    );
  }

  const activeTab: TabId = surface === 'profile' ? 'profile' : 'recommend';

  return (
    <view className="CommerceShell">
      {surface === 'profile' ? (
        <ProfilePage />
      ) : (
        <RecommendHome onOpenNova={() => setSurface('detail')} />
      )}
      <BottomTabs activeTab={activeTab} onSelect={handleTabSelect} />
    </view>
  );
}

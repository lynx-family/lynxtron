import { useState, useRef, useEffect, useCallback } from '@lynx-js/react';
import './App.css';
import { HomePage } from './HomePage.tsx';
import { NewTab } from './NewTab.tsx';
import LynxIcon from './icons/lynxlogo.png?inline';
import PlusIcon from './icons/plus.png?inline';
import BackIcon from './icons/back.png?inline';
import ForwardIcon from './icons/forward.png?inline';
import RefreshIcon from './icons/refresh.png?inline';
import MaximizeIcon from './icons/maximize.png?inline';
import BlurIcon from './icons/blur.png?inline';
import CloseIcon from './icons/close.png?inline';

export function App() {
  const [tabs, setTabs] = useState<
    {
      id: string;
      icon: 'lynx' | 'lynxtron';
      title: string;
      input_value?: string;
      actual_url?: string;
      favicon?: string;
    }[]
  >([{ id: 'tab-1', icon: 'lynxtron', title: 'New Tab', input_value: '' }]);
  const [selectedTabId, setSelectedTabId] = useState<string>('tab-1');
  const [inputValue, setInputValue] = useState('');

  // Use ref to store webview element references
  const webviewRefs = useRef<Record<string, any>>({});

  // Use ref to store shared interval
  const sharedIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const lastTopBarTapTimeRef = useRef<number>(0);
  const topBarTapCountRef = useRef<number>(0);
  const topBarTapTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const DOUBLE_TAP_WINDOW_MS = 300;
  const [showCloseIcon, setShowCloseIcon] = useState(false);

  // Create shared interval for current tab
  useEffect(() => {
    // Clear existing interval if any
    if (sharedIntervalRef.current) {
      clearInterval(sharedIntervalRef.current);
    }

    // Start shared interval
    sharedIntervalRef.current = setInterval(() => {
      if (!selectedTabId) return;

      // Get current tab
      const currentTab = tabs.find((tab) => tab.id === selectedTabId);
      if (!currentTab) return;

      // Get webview reference for current tab
      const webviewElement = webviewRefs.current[selectedTabId];
      if (!webviewElement) return;

      // Code to get page info
      const pageInfoCode = `
        (function(){
          try{
            var send = function(payload){
              try{
                var s = JSON.stringify(payload);
                if(window.cefQuery){
                  window.cefQuery({request:'LyNxSig_'+s,persistent:false,onSuccess:function(){},onFailure:function(){}});
                }else{
                  window.postMessage(s,'*');
                  try{
                    var h = '#LyNxSig_'+encodeURIComponent(s);
                    if(location.hash!==h){ location.hash = h; }
                  }catch(_){}
                }
              }catch(_){}
            };
            var t = document.title;
            // Get URL and remove hash part
            var u = window.location.href.split('#')[0];
            var payload = { type: 'page_info', page_title: t, page_url: u, timestamp: Date.now() };
            send(payload);
          }catch(_){}
        })();
      `;

      // Execute code in webview
      webviewElement
        .invoke({
          method: 'eval',
          params: {
            func: pageInfoCode,
          },
          success: () => {
            console.log('Shared timer: Page info eval success');
          },
          fail: (res: any) => {
            console.log('Shared timer: Page info eval fail', res);
          },
        })
        .exec();
    }, 50000); // Check every 5 second

    // Clean up interval on unmount or when dependencies change
    return () => {
      if (sharedIntervalRef.current) {
        clearInterval(sharedIntervalRef.current);
      }
    };
  }, [selectedTabId, tabs]);

  const handleAddTab = (
    icon: 'lynx' | 'lynxtron' = 'lynxtron',
    title: string = 'New Tab'
  ) => {
    // Generate unique ID using timestamp to avoid duplicates
    const newTabId = `tab-${Date.now()}`;
    const newTab = { id: newTabId, icon, title, input_value: '' };
    setTabs([...tabs, newTab]);
    setInputValue('');
    setSelectedTabId(newTabId);
  };

  const openLynxJSWebsite = () => {
    if (!selectedTabId) return;
    setTabs((prevTabs) =>
      prevTabs.map((tab) => {
        if (tab.id === selectedTabId) {
          return {
            ...tab,
            icon: 'lynx',
            title: 'Lynx',
            input_value: 'https://lynxjs.org/',
          };
        }
        return tab;
      })
    );
  };

  const handleHideTap = useCallback(() => {
    // console.log('[App] handleHideTap triggered', NativeModules.nodejs.exposed);
    NativeModules.bridge.call(
      'hideWindow',
      {
        // message: NativeModules.nodejs.exposed.echo('Hello from Lynxtron!'),
      },
      () => {
        console.log('[App] bridge.request callback fired');
      }
    );
  }, []);

  const handleFullScreenTap = useCallback(() => {
    // console.log('[App] handleFullScreenTap triggered', NativeModules.nodejs.exposed);
    console.log('[App] handleFullScreenTap triggered', selectedTabId);
    NativeModules.bridge.call(
      'fullScreenWindow',
      {
        // message: NativeModules.nodejs.exposed.echo('Hello from Lynxtron!'),
      },
      () => {
        console.log('[App] bridge.request callback fired');
      }
    );
  }, []);

  const handleMaximizeTap = useCallback(() => {
    // console.log('[App] handleMaximizeTap triggered', NativeModules.nodejs.exposed);
    console.log('[App] handleMaximizeTap triggered', selectedTabId);
    NativeModules.bridge.call(
      'maximizeWindow',
      {
        // message: NativeModules.nodejs.exposed.echo('Hello from Lynxtron!'),
      },
      () => {
        console.log('[App] bridge.request callback fired');
      }
    );
  }, []);

  const handleCloseTap = useCallback(() => {
    // console.log('[App] handleCloseTap triggered', NativeModules.nodejs.exposed);
    NativeModules.bridge.call(
      'closeWindow',
      {
        // message: NativeModules.nodejs.exposed.echo('Hello from Lynxtron!'),
      },
      () => {
        console.log('[App] bridge.request callback fired');
      }
    );
  }, []);

  // Refresh currently selected webview
  const handleRefresh = () => {
    if (!selectedTabId) return;

    // Get currently selected tab
    const currentTab = tabs.find((tab) => tab.id === selectedTabId);
    if (!currentTab) {
      console.log('Current tab not found for ID:', selectedTabId);
      console.log(
        'Available tabs:',
        tabs.map((tab) => tab.id)
      );
      return;
    }

    if (currentTab.input_value) {
      const webviewElement = webviewRefs.current[selectedTabId];
      if (webviewElement && webviewElement.invoke) {
        webviewElement
          .invoke({
            method: 'reload',
            success: function (res) {
              console.log(res);
            },
            fail: function (res) {
              console.log(res.code, res.data);
            },
          })
          .exec();
      }
    }
  };

  const handleBackward = () => {
    if (!selectedTabId) return;
    console.log('handleBackward', selectedTabId);
    const webviewElement = webviewRefs.current[selectedTabId];
    if (webviewElement && webviewElement.invoke) {
      const jsCode = `
        window.history.back();
      `;
      webviewElement
        .invoke({
          method: 'eval',
          params: {
            func: jsCode,
          },
          success: function () {},
          fail: function (res: any) {
            console.log('eval fail', res);
          },
        })
        .exec();
    }
  };

  const handleForward = () => {
    if (!selectedTabId) return;
    const webviewElement = webviewRefs.current[selectedTabId];
    if (webviewElement && webviewElement.invoke) {
      const jsCode = `
        window.history.forward();
      `;
      webviewElement
        .invoke({
          method: 'eval',
          params: {
            func: jsCode,
          },
          success: function () {},
          fail: function (res: any) {
            console.log('eval fail', res);
          },
        })
        .exec();
    }
  };

  const handleTabTap = (tabId: string) => {
    setSelectedTabId(tabId);

    // Find selected tab
    const selectedTab = tabs.find((tab) => tab.id === tabId);
    if (selectedTab) {
      // Use actual_url if it exists, otherwise use input_value
      setInputValue(selectedTab.actual_url || selectedTab.input_value || '');
    }
  };

  const handleInput = (event: InputEvent) => {
    'background only';
    const currentValue = event.detail.value.trim();
    // Update input_value for currently selected tab
    setTabs((prevTabs) =>
      prevTabs.map((tab) => {
        if (tab.id === selectedTabId) {
          return { ...tab, input_value: currentValue };
        }
        return tab;
      })
    );

    setInputValue(currentValue);
  };

  const handleRemoveTab = (tabIdToRemove: string) => {
    // Filter out the tab to be removed
    const updatedTabs = tabs.filter((tab) => tab.id !== tabIdToRemove);

    // Update tabs array
    setTabs(updatedTabs);

    // If removing currently selected tab
    if (selectedTabId === tabIdToRemove) {
      if (updatedTabs.length > 0) {
        // If there are remaining tabs, select the last one in the new array
        const removedIndex = tabs.findIndex((tab) => tab.id === tabIdToRemove);
        const newSelectedIndex = Math.min(removedIndex, updatedTabs.length - 1);
        const newSelectedTabId = updatedTabs[newSelectedIndex].id;
        setSelectedTabId(newSelectedTabId);
      } else {
        // If no tabs left, can close the app
        // In Lynx environment, specific API can be used to close the app
        // Here simply clear selection state
        NativeModules.bridge.call('close', {}, (res: any) => {
          console.log(res);
        });
        setSelectedTabId('');
      }
    }
  };

  const handleOpenWindow = (event: any) => {
    const raw = event && event.detail ? event.detail.url : '';
    let u = String(raw || '');
    u = u.replace(/[`'"]/g, '').trim();
    if (!u) return;
    const newTabId = `tab-${Date.now()}`;
    const newTab = { id: newTabId, icon: 'lynxtron', title: u, input_value: u };
    setTabs([...tabs, newTab]);
    setSelectedTabId(newTabId);
    setInputValue(u);
  };

  const handleTopBarTap = useCallback(
    (e?: any) => {
      try {
        if (
          e &&
          e.target &&
          e.currentTarget &&
          e.target.uid &&
          e.currentTarget.uid &&
          e.target.uid !== e.currentTarget.uid
        ) {
          return;
        }
      } catch (_) {}
      const now = Date.now();
      if (now - lastTopBarTapTimeRef.current <= DOUBLE_TAP_WINDOW_MS) {
        topBarTapCountRef.current = topBarTapCountRef.current + 1;
      } else {
        topBarTapCountRef.current = 1;
      }
      lastTopBarTapTimeRef.current = now;
      if (topBarTapTimeoutRef.current) {
        clearTimeout(topBarTapTimeoutRef.current);
        topBarTapTimeoutRef.current = null;
      }
      if (topBarTapCountRef.current >= 2) {
        topBarTapCountRef.current = 0;
        handleMaximizeTap();
        return;
      }
      topBarTapTimeoutRef.current = setTimeout(() => {
        topBarTapCountRef.current = 0;
        topBarTapTimeoutRef.current = null;
      }, DOUBLE_TAP_WINDOW_MS);
    },
    [handleMaximizeTap]
  );

  return (
    <view clip-radius="true" className="page-root">
      <view className="top-bar" bindtap={handleTopBarTap}>
        {SystemInfo?.platform === 'macOS' && (
          <view
            className="traffic-group"
            bindmouseenter={() => setShowCloseIcon(true)}
            bindmouseleave={() => setShowCloseIcon(false)}
          >
            <view
              clip-radius="true"
              className="traffic-red"
              bindtap={handleCloseTap}
            >
              <image
                src={CloseIcon}
                style={{
                  margin: 'auto',
                  width: '5px',
                  aspectRatio: '1',
                  visibility: showCloseIcon ? 'visible' : 'hidden',
                }}
              />
            </view>
            <view
              clip-radius="true"
              className="traffic-yellow"
              bindtap={handleHideTap}
            >
              <image
                src={BlurIcon}
                style={{
                  margin: 'auto',
                  width: '8px',
                  aspectRatio: '1',
                  visibility: showCloseIcon ? 'visible' : 'hidden',
                }}
              />
            </view>
            <view
              clip-radius="true"
              className="traffic-green"
              bindtap={handleFullScreenTap}
            >
              <image
                src={MaximizeIcon}
                style={{
                  margin: 'auto',
                  width: '8px',
                  aspectRatio: '1',
                  visibility: showCloseIcon ? 'visible' : 'hidden',
                }}
              />
            </view>
          </view>
        )}

        {SystemInfo?.platform === 'windows' && (
          <view style={{ width: '20px' }}></view>
        )}

        <view className="tabs-container">
          {tabs.map((tab) => (
            <NewTab
              key={tab.id}
              tabId={tab.id}
              isSelected={tab.id === selectedTabId}
              icon={tab.icon}
              title={tab.title}
              favicon={tab.favicon}
              onTap={() => handleTabTap(tab.id)}
              onClose={() => handleRemoveTab(tab.id)}
            />
          ))}
        </view>

        <view className="plus-icon-container" bindtap={handleAddTab}>
          <image src={PlusIcon} className="plus-icon" />
        </view>

        {SystemInfo?.platform === 'windows' && (
          <view
            className="window-controls-group"
            bindmouseenter={() => setShowWindowControls(true)}
            bindmouseleave={() => setShowWindowControls(false)}
          >
            <view className="window-control-button" bindtap={handleHideTap}>
              <text
                className="window-control-symbol"
                style={{ fontSize: '15px' }}
              >
                —
              </text>
            </view>
            <view className="window-control-button" bindtap={handleMaximizeTap}>
              <text
                className="window-control-symbol"
                style={{ fontSize: '25px', top: '-2px' }}
              >
                □
              </text>
            </view>
            <view
              className="window-control-button window-control-close"
              bindtap={handleCloseTap}
            >
              <text
                className="window-control-symbol"
                style={{ fontSize: '25px' }}
              >
                ×
              </text>
            </view>
          </view>
        )}
      </view>

      <view className="toolbar">
        <view clip-radius="true" className="toolbar-row">
          <view style={{ display: 'flex', width: '73px', flexShrink: '0' }}>
            <view bindtap={handleBackward}>
              <image src={BackIcon} className="icon-12" />
            </view>
            <view bindtap={handleForward}>
              <image src={ForwardIcon} className="icon-12-rotated" />
            </view>
            <view bindtap={handleRefresh}>
              <image src={RefreshIcon} className="refresh-icon" />
            </view>
          </view>
          <view clip-radius="true" className="address-bar">
            <image src={LynxIcon} className="padlock-icon" />
            <view className="separator-container">
              <input
                className="separator-input"
                value={inputValue}
                bindconfirm={handleInput}
              />
            </view>
            {/* <image src={ShareIcon} className="share-icon" />
            <image src={StarIcon} className="star-icon" /> */}
          </view>

          {/* <image src={MoreIcon} className="more-icon" /> */}
        </view>

        <view className="shortcut-row">
          <view className="shortcut-chip" bindtap={() => openLynxJSWebsite()}>
            <image
              src={LynxIcon}
              style={{ top: '1px' }}
              className="shortcut-icon"
            />
            <text className="shortcut-label-1">Lynx</text>
          </view>
        </view>
      </view>

      <view className="content-area">
        {tabs.map((tab) => (
          <view
            key={tab.id}
            className="tab-content"
            style={{ zIndex: selectedTabId === tab.id ? 1 : 0 }}
          >
            {!tab.input_value && (
              <HomePage onConvertToLynxTab={openLynxJSWebsite} />
            )}
            {tab.input_value && (
              <x-webview
                className="webview-container"
                id={`webview-${tab.id}`}
                src={tab.input_value}
                use-osr={true}
                bounces={true}
                enable-debug={true}
                ref={(el) => {
                  if (el) {
                    webviewRefs.current[tab.id] = el;
                  }
                }}
                bindopenwindow={(e: any) => {
                  handleOpenWindow(e);
                }}
                bindload={() => {
                  // Initial page info fetch
                  const initJsCode = `
                    (function(){
                      try{
                        var send = function(payload){
                          try{
                            var s = JSON.stringify(payload);
                            if(window.cefQuery){
                              window.cefQuery({request:'LyNxSig_'+s,persistent:false,onSuccess:function(){},onFailure:function(){}});
                            }else{
                              window.postMessage(s,'*');
                              try{
                                var h = '#LyNxSig_'+encodeURIComponent(s);
                                if(location.hash!==h){ location.hash = h; }
                              }catch(_){}
                            }
                          }catch(_){}
                        };
                        var t = document.title;
                        // Get URL and remove hash part
                        var u = window.location.href.split('#')[0];
                        var payload = { type: 'page_info', page_title: t, page_url: u };
                        send(payload);
                      }catch(_){}
                    })();
                  `;

                  webviewRefs.current[tab.id]
                    .invoke({
                      method: 'eval',
                      params: {
                        func: initJsCode,
                      },
                      success: () => {
                        console.log('Initial page info eval success');
                      },
                      fail: (res: any) => {
                        console.log('Initial page info eval fail', res);
                      },
                    })
                    .exec();
                }}
                bindmessage={(e: any) => {
                  console.log('bindmessage start', e);
                  const payloadRaw = e && e.detail ? e.detail.msg : '';
                  if (!payloadRaw) return;

                  let title = '';
                  let url = '';
                  let isPageInfoMessage = false;

                  if (typeof payloadRaw === 'string') {
                    console.log('bindmessage string', e);
                    let s = payloadRaw;
                    try {
                      s = decodeURIComponent(s);
                    } catch (_) {}
                    try {
                      const obj = JSON.parse(s);
                      // Filter initialization events
                      if (obj && obj.event === 'alreadyInitialized') {
                        return;
                      }
                      // Handle page info message (new format)
                      if (obj && obj.type === 'page_info' && obj.page_title) {
                        title = obj.page_title;
                        url = obj.page_url || '';
                        isPageInfoMessage = true;
                      } else if (
                        obj &&
                        obj.type === 'page_title' &&
                        obj.page_title
                      ) {
                        // Handle old title-only format
                        title = obj.page_title;
                        isPageInfoMessage = true;
                      } else if (obj && obj.detail) {
                        // Compatible with old format
                        title = obj.detail.msg ?? '';
                        isPageInfoMessage = true;
                      } else if (obj && obj.title) {
                        // Compatible with other title formats
                        title = obj.title;
                        isPageInfoMessage = true;
                      }
                    } catch {
                      // Non-JSON string, might be direct title
                      title = s;
                      isPageInfoMessage = true;
                    }
                  } else if (payloadRaw && typeof payloadRaw === 'object') {
                    // Filter initialization events
                    if (payloadRaw.event === 'alreadyInitialized') {
                      return;
                    }
                    // Handle page info message (new format)
                    if (
                      payloadRaw.type === 'page_info' &&
                      payloadRaw.page_title
                    ) {
                      title = payloadRaw.page_title;
                      url = payloadRaw.page_url || '';
                      isPageInfoMessage = true;
                    } else if (
                      payloadRaw.type === 'page_title' &&
                      payloadRaw.page_title
                    ) {
                      // Handle old title-only format
                      title = payloadRaw.page_title;
                      isPageInfoMessage = true;
                    } else if (payloadRaw.detail?.msg) {
                      // Compatible with old format
                      title = payloadRaw.detail.msg;
                      isPageInfoMessage = true;
                    } else if (payloadRaw.title) {
                      // Compatible with other title formats
                      title = payloadRaw.title;
                      isPageInfoMessage = true;
                    }
                  }

                  if (isPageInfoMessage && title) {
                    console.log('Page info received:', { title, url });

                    // Find current tab to check if anything has changed
                    const currentTab = tabs.find((t) => t.id === tab.id);
                    if (!currentTab) return;

                    // Check if title or URL has changed
                    const titleChanged = currentTab.title !== title;
                    const urlChanged = url && currentTab.actual_url !== url;

                    // Only update if something has changed
                    if (titleChanged || urlChanged) {
                      setTabs((prev) =>
                        prev.map((t) => {
                          if (t.id === tab.id) {
                            const updatedTab = { ...t };
                            if (titleChanged) {
                              updatedTab.title = title;
                            }
                            if (urlChanged) {
                              updatedTab.actual_url = url;
                            }
                            return updatedTab;
                          }
                          return t;
                        })
                      );

                      // Update input field if this is the selected tab and URL has changed
                      if (tab.id === selectedTabId && urlChanged) {
                        setInputValue(url);
                      }
                    }
                  }
                }}
              />
            )}
          </view>
        ))}
      </view>
    </view>
  );
}

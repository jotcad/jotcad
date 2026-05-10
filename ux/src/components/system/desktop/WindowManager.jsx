import { For, Show } from 'solid-js';
import { openWindows } from '../../../lib/state/AppState';
import { Window } from './Window';
import { JotNode } from '../../editor/JotNode';
import { CatalogNode } from '../../discovery/CatalogNode';
import { MeshGraphApp } from '../../discovery/MeshGraphApp';
import { Console } from '../../system/Console';
import { FolderWindow } from './FolderWindow';
import { SettingsApp } from '../../system/SettingsApp';

export const WindowManager = (props) => {
  // Filter windows based on whether they should be in the spatial group or fixed viewport
  const filteredWindows = () => openWindows.filter(win => {
    const isMax = win.isMaximized || false;
    // Spatial layer shows floating windows
    if (props.isSpatial) return !isMax;
    // Non-spatial layer shows maximized windows
    return isMax;
  });

  return (
    <div class={`${props.isSpatial ? 'absolute' : 'fixed'} inset-0 pointer-events-none z-50`}>
      <For each={filteredWindows()} by="id">
        {(win) => {
          return (
            <Window data={win} ignoreZoom={!props.isSpatial}>
              <Show when={win.type === 'editor'}>
                <JotNode data={win} isWindowed={true} />
              </Show>
              <Show when={win.type === 'catalog'}>
                <CatalogNode isWindowed={true} />
              </Show>
              <Show when={win.type === 'mesh'}>
                <MeshGraphApp />
              </Show>
              <Show when={win.type === 'console'}>
                 <Console isWindowed={true} />
              </Show>
              <Show when={win.type === 'folder'}>
                 <FolderWindow data={win} />
              </Show>
              <Show when={win.type === 'settings'}>
                 <SettingsApp />
              </Show>
            </Window>
          );
        }}
      </For>
    </div>
  );
};

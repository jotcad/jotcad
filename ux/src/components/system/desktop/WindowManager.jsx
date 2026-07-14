import { For, Show } from 'solid-js';
import { openWindows } from '../../../lib/state/DesktopState';
import { Window } from './Window';
import { JotNode } from '../../editor/JotNode';
import { CatalogNode } from '../../discovery/CatalogNode';
import { MeshGraphApp } from '../../discovery/MeshGraphApp';
import { Console } from '../../system/Console';
import { FolderWindow } from './FolderWindow';
import { SettingsApp } from '../../system/SettingsApp';
import { CameraWindow } from './CameraWindow';
import { MaterialsApp } from '../../system/MaterialsApp';
import { SkusApp } from '../../system/SkusApp';
import { InstrumentsApp } from '../../system/InstrumentsApp';
import { RgbLedWindow } from './RgbLedWindow';


export const WindowManager = (props) => {
  return (
    <div class="absolute inset-0 pointer-events-none z-50">
      <For each={openWindows} by="id">
        {(win) => {
          return (
            <Window data={win}>
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
              <Show when={win.type === 'materials'}>
                 <MaterialsApp isWindowed={true} />
              </Show>
              <Show when={win.type === 'skus'}>
                 <SkusApp isWindowed={true} />
              </Show>
              <Show when={win.type === 'instruments'}>
                 <InstrumentsApp />
              </Show>
              <Show when={win.type === 'vision_debug'}>
                 <CameraWindow />
              </Show>
              <Show when={win.type === 'rgb_led'}>
                 <RgbLedWindow />
              </Show>
            </Window>
          );
        }}
      </For>
    </div>
  );
};

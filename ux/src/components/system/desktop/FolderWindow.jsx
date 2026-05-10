import { For } from 'solid-js';
import { desktopIcons } from '../../../lib/state/AppState';
import { DesktopIcon } from './DesktopIcon';

export const FolderWindow = (props) => {
  // Filters icons that belong to this folder
  const children = () => props.data.children || [];

  return (
    <div class="w-full h-full p-6 bg-black/40">
      <div class="grid grid-cols-3 md:grid-cols-4 gap-4 auto-rows-min">
        <For each={children()}>
          {(child) => <DesktopIcon data={child} isNested={true} />}
        </For>
      </div>
    </div>
  );
};

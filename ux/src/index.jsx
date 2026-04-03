import { render } from 'solid-js/web';
import { Canvas } from './components/Canvas';
import './index.css';

function App() {
  return (
    <div class="w-screen h-screen">
      <Canvas />
    </div>
  );
}

const root = document.getElementById('root');
if (root) {
  render(() => <App />, root);
}

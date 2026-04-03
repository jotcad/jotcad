/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        blackboard: '#1a1a1a',
        node: '#2a2a2a',
        wire: '#4a4a4a',
        pending: '#fbbf24',
        available: '#34d399',
        provisioning: '#60a5fa',
      }
    },
  },
  plugins: [],
}

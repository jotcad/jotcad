# JotCAD Web Viewer

This subproject provides a simple, single-page web application for viewing
JotCAD models serialized in the JOT format. It leverages Three.js for
interactive 3D rendering and uses modern browser features like `importmap` for
dependency management.

## Features

- Fetches JOT-formatted model data from a web server.
- Parses JOT data and converts it into Three.js geometries.
- Displays the 3D model in an interactive orbital view.
- Supports basic material rendering based on JotCAD shape tags (e.g., color).

## Setup and Running

To get this web viewer up and running, follow these steps:

1.  **Start the JotCAD Server (Backend):** The web viewer relies on a JotCAD
    server to process code and generate JOT files.

    - **Navigate to the `server/` directory:**
      ```bash
      cd ../server/
      ```
    - **Install Dependencies:**
      ```bash
      npm install
      ```
    - **Start the server:**
      ```bash
      npm start
      ```
      This will typically start the JotCAD server on `http://localhost:3000`.
      You will see output in your terminal indicating the server address. Keep
      this terminal open.

2.  **Start the Web Viewer (Frontend):**

    - **Navigate to the `web/` directory:**

      ```bash
      cd ../web/
      ```

    - **Install Dependencies:** This project uses `http-server` as a development
      server. Install it via npm:

      ```bash
      npm install
      ```

    - **Start the Development Server:** Use the `start` script defined in
      `package.json` to launch the server:
      ```bash
      npm start
      ```
      This will typically start a server on `http://localhost:8080`. You will
      see output in your terminal indicating the server address. Keep this
      terminal open.

3.  **View the Application:** Open your web browser and navigate to the address
    provided by the `http-server` (e.g., `http://localhost:8080`).

    You should see a 3D cube rendered in the browser. This cube is loaded from
    the `example.jot` file located in the `web/` directory, but fetched via the
    JotCAD server. You can interact with the cube using your mouse (click and
    drag to rotate, scroll to zoom).

## Providing a Custom JOT File

The application is configured to fetch a file named `example.jot` from the root
of the server. To view your own JotCAD model:

1.  **Generate your JOT file:** Use the JotCAD CLI or library to serialize your
    `Shape` object into a JOT-formatted string. Save this string to a file
    (e.g., `my_model.jot`).
2.  **Place the file:** Copy your `my_model.jot` file into the `web/` directory.
3.  **Update `web/main.js`:** Open `web/main.js` and change the `fetch` call to
    point to your file:
    ```javascript
    // In web/main.js
    const response = await fetch('/my_model.jot'); // Change '/example.jot' to '/my_model.jot'
    ```
4.  **Restart the server:** If the server was already running, stop it
    (`Ctrl+C`) and start it again (`npm start`) to pick up the changes.
5.  **Refresh your browser:** Your custom model should now be displayed.

## Project Structure

- `index.html`: The main HTML file for the single-page application.
- `main.js`: The application's entry point, responsible for setting up the
  Three.js scene, fetching JOT data, and initiating the rendering process.
- `jot_threejs_viewer.js`: Contains the core logic for parsing JOT data and
  converting it into Three.js objects for rendering. This file is decoupled from
  the main JotCAD library.
- `example.jot`: A sample JOT file representing a simple cube, used for
  demonstration.
- `package.json`: Defines project metadata and development dependencies (like
  `http-server`).
- `.babelrc`, `.eslintrc.cjs`: Configuration files for Babel and ESLint.

---

# TrackTag Operations Dashboard

This React/Vite dashboard is the browser operations console for TrackTag Navigation.
It uses a local bridge at `127.0.0.1:8080`; the browser never talks to Ignition
Transport itself.

```bash
npm ci
npm run dev
```

For the launcher-served production dashboard, run `npm run build`, then launch the
simulator with `--web` and a navigation map:

```bash
../build/track-tag-navigation --run --web --map maps/junction_track.json
```

The visual system and initial project structure are based on
[shadcndashboard](https://github.com/shadcndashboard/shadcndashboard), licensed MIT.
Its [MIT licence](https://github.com/shadcndashboard/shadcndashboard/blob/main/LICENSE)
applies to the imported design basis; this project retains its own licence at the root.

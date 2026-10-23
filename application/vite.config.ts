import { defineConfig } from 'vite'
import path from 'node:path'
import electron from 'vite-plugin-electron/simple'
import react from '@vitejs/plugin-react'

const aliasConfig = {
  '#types': path.resolve(__dirname, 'src/shared/types'),
  '#services': path.resolve(__dirname, 'src/backend/services'), 
  '#model': path.resolve(__dirname, 'src/backend/model'),
  '#shared': path.resolve(__dirname, 'src/shared'),

  '#components': path.resolve(__dirname, 'src/frontend/components'),
  '#hooks': path.resolve(__dirname, 'src/frontend/hooks'),
  '#styles': path.resolve(__dirname, 'src/frontend/styles'),
  '#assets': path.resolve(__dirname, 'src/frontend/assets'),
  '#contexts': path.resolve(__dirname, 'src/frontend/contexts'),
  '#pages': path.resolve(__dirname, 'src/frontend/pages'),
  '#utils': path.resolve(__dirname, 'src/frontend/utils'),
  '#models': path.resolve(__dirname, 'src/frontend/models'),
  
}

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    react(),
    electron({  
      main: {
        entry: 'src/backend/main.ts',
        vite: {
          build: {
            rollupOptions: {
              external: ['sqlite3', 'serialport', '@prisma/client', '.prisma/client'], 
            }
          },
          resolve: { alias: aliasConfig } 
        }
      },
      preload: {
        input: path.join(__dirname, 'src/frontend/preload.ts'),
        vite: {
          resolve: { alias: aliasConfig } 
        }
      },
      renderer: process.env.NODE_ENV === 'test' ? undefined : {},
    }),
  ],
  resolve: {
    alias: aliasConfig,
  },
})
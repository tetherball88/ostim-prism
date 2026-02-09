import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve, dirname } from 'path'
import { fileURLToPath } from 'url'
import { viteSingleFile } from "vite-plugin-singlefile"
import { viteStaticCopy } from 'vite-plugin-static-copy'

const __dirname = dirname(fileURLToPath(import.meta.url))

export default defineConfig({
  plugins: [
    react(),
    viteSingleFile(),
  ],
  build: {
    outDir: resolve(__dirname, '../PrismaUI/views/OStim'),
    emptyOutDir: true,
  },
  // publicDir: resolve(__dirname, '../data/Interface/OStim/icons')
})

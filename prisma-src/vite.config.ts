import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve, dirname } from 'path'
import { fileURLToPath } from 'url'
import { viteStaticCopy } from 'vite-plugin-static-copy'

const __dirname = dirname(fileURLToPath(import.meta.url))

export default defineConfig({
  base: './',
  plugins: [
    react(),
    {
      name: 'remove-module-attrs',
      transformIndexHtml(html) {
        const scripts: string[] = []
        html = html
          .replace(/ type="module"/g, '')
          .replace(/ crossorigin/g, '')
          .replace(/<script\b[^>]*src="[^"]*"[^>]*><\/script>/g, (match) => {
            scripts.push(match)
            return ''
          })
        return html.replace('</body>', `  ${scripts.join('\n  ')}\n</body>`)
      }
    },
    viteStaticCopy({
      targets: [
        { src: 'src/variables.css', dest: '' }
      ]
    }),
  ],
  build: {
    outDir: resolve(__dirname, '../PrismaUI/views/OStim'),
    emptyOutDir: true,
    minify: false,
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        format: 'iife',
      },
    },
  },
  // publicDir: resolve(__dirname, '../data/Interface/OStim/icons')
})

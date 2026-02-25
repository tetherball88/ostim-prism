/** True when running inside the Skyrim CEF host (production build). */
export const isInGame = (): boolean => import.meta.env.PROD;
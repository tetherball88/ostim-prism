import { ChangeEvent, FocusEvent, useEffect, useRef } from "react";
import { useOStimStore } from "../../store";
import "./SearchMenu.styles.css";
import { isInGame } from "../../utils/isInGame";
import { SearchResult } from "../../types";

const SEARCH_DEBOUNCE_MS = 150;
const mockResults: SearchResult[] = [
    { sceneId: "scene1", name: "Beach Sunset", actorCount: 5 },
    { sceneId: "scene2", name: "City Rooftop", actorCount: 3 },
    { sceneId: "scene3", name: "Forest Clearing", actorCount: 4 },
    { sceneId: "scene4", name: "Mountain Peak", actorCount: 2 },
    { sceneId: "scene5", name: "Dungeon Entrance", actorCount: 6 },
    { sceneId: "scene6", name: "Castle Courtyard", actorCount: 4 },
    { sceneId: "scene7", name: "Village Market", actorCount: 8 },
    { sceneId: "scene8", name: "River Crossing", actorCount: 3 },
    { sceneId: "scene9", name: "Abandoned Mine", actorCount: 5 },
    { sceneId: "scene10", name: "Ancient Ruins", actorCount: 7 },
];


export const SearchMenu = () => {
  const query = useOStimStore(state => state.search.query);
  const results = useOStimStore(state => state.search.results);
  const activeIndex = useOStimStore(state => state.search.activeIndex);
  const updateSearchQuery = useOStimStore(state => state.updateSearchQuery);
  const setSearchActiveIndex = useOStimStore(state => state.setSearchActiveIndex);
  const selectSearchResult = useOStimStore(state => state.selectSearchResult);
  const focusBlock = useOStimStore(state => state.focusBlock);
  const keys = useOStimStore(state => state.keys);

  const inputRef = useRef<HTMLInputElement | null>(null);
  const listRef = useRef<HTMLDivElement | null>(null);
  const itemRefs = useRef<Array<HTMLDivElement | null>>([]);
  const debounceRef = useRef<number | null>(null);

  useEffect(() => {
      let lastKey: null | number = null;
      const handleKeyDown = (event: KeyboardEvent) => {
        console.log("Key pressed:", event.keyCode);
        lastKey = event.keyCode;
      }
      const numKeys: number[] = [keys.keyDown, keys.keyUp, keys.keyLeft, keys.keyRight, keys.keyYes];
      console.log("Current keys:", keys);
      const handleBeforeInput = (event: InputEvent) => {
        if (lastKey !== null && numKeys.some(k => k === lastKey)) {
          console.log("Preventing input for key:", lastKey);
          event.preventDefault();
        }
      }
      window.addEventListener("keydown", handleKeyDown, { capture: true });
      window.addEventListener("beforeinput", handleBeforeInput, { capture: true });
      return () => {
        window.removeEventListener("keydown", handleKeyDown, { capture: true });
        window.removeEventListener("beforeinput", handleBeforeInput, { capture: true });
      }
  }, [])

  useEffect(() => {
    if(focusBlock === 'menu') {
      inputRef.current?.focus();
    } else {
      inputRef.current?.blur();
    }
  }, [focusBlock]);

  useEffect(() => {
    return () => {
      if (debounceRef.current !== null) {
        window.clearTimeout(debounceRef.current);
      }

      window.sendAction?.(JSON.stringify({ action: "setTextInputFocus", payload: { focused: false } }));
    };
  }, []);

  useEffect(() => {
    const listEl = listRef.current;
    const itemEl = itemRefs.current[activeIndex];
    if (!listEl || !itemEl) {
      return;
    }

    const listRect = listEl.getBoundingClientRect();
    const itemRect = itemEl.getBoundingClientRect();

    if (itemRect.top < listRect.top) {
      listEl.scrollTop -= (listRect.top - itemRect.top);
    } else if (itemRect.bottom > listRect.bottom) {
      listEl.scrollTop += (itemRect.bottom - listRect.bottom);
    }
  }, [activeIndex, results.length]);

  const sendQuery = (value: string) => {
    if(isInGame()) {
        window.sendAction?.(JSON.stringify({
            action: "searchQuery",
            payload: { query: value }
        }));
    } else {
        console.log("Simulating search results for query:", value);
        window.updateSearchResults(mockResults.filter(r => r.name.toLowerCase().includes(value.toLowerCase()) || r.sceneId.toLowerCase().includes(value.toLowerCase())));
    }
  };

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const value = event.target.value;
    updateSearchQuery(value);

    if (debounceRef.current !== null) {
      window.clearTimeout(debounceRef.current);
    }

    debounceRef.current = window.setTimeout(() => {
      sendQuery(value);
    }, SEARCH_DEBOUNCE_MS);
  };

  const handleSelect = (index: number) => {
    setSearchActiveIndex(index);
    selectSearchResult(index);
  };

  const handleInputFocus = () => {
    if (isInGame()) {
      window.sendAction?.(JSON.stringify({ action: "setTextInputFocus", payload: { focused: true } }));
    }
  };

  const handleInputBlur = (e: FocusEvent<HTMLInputElement>) => {
    if (focusBlock === 'menu' && !e.relatedTarget) {
        // Prevent losing focus when clicking non-focusable elements (like scrollbar)
        e.target.focus();
        return;
    }

    if (isInGame()) {
      window.sendAction?.(JSON.stringify({ action: "setTextInputFocus", payload: { focused: false } }));
    }
  };

  return (
    <div className="search-panel">
      <div className="search-header">
        <span className="search-title">Scene Search</span>
        <span className="search-count">{results.length} found</span>
      </div>

      <input
        ref={inputRef}
        className="search-input"
        type="text"
        placeholder="Type a scene name..."
        value={query}
        onChange={handleChange}
        onFocus={handleInputFocus}
        onBlur={handleInputBlur}
      />

      <div className="search-results" role="listbox" ref={listRef}>
        {results.length === 0 && query.trim().length > 0 && (
          <div className="search-empty">No matches.</div>
        )}
        {results.length === 0 && query.trim().length === 0 && (
          <div className="search-empty">Start typing to search scenes.</div>
        )}

        {results.map((result, index) => (
          <div
            key={result.sceneId}
            className={`search-result ${index === activeIndex ? "active" : ""}`}
            ref={(node) => { itemRefs.current[index] = node; }}
            onMouseEnter={() => setSearchActiveIndex(index)}
            onClick={() => handleSelect(index)}
          >
            <div className="search-name">{result.name}</div>
            <div className="search-meta">
              <span className="search-scene">{result.sceneId}</span>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};

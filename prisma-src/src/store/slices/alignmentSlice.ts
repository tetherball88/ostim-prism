// src/store/slices/alignmentSlice.ts
import { alignmentFields } from '../../alignmentConfig';
import { AlignmentPayload } from '../../types';
import { StoreSlice } from '../types';

// const debounceUpdateAlignment = (() => {
//     let timeouts: Record<string, number> = {};
//     return (field: string, value: number, actorIndex: number) => {
//         if (timeouts[field]) {
//             window.clearTimeout(timeouts[field]);
//         }
//         timeouts[field] = window.setTimeout(() => {
//             if(typeof value !== 'number' || isNaN(value)) {
//                 value = 0;
//             }
//             console.log("Debounced alignment update", field, value, actorIndex);
            
//             delete timeouts[field];
//         }, 1000);
//     }
// })();

export const createAlignmentSlice: StoreSlice<import('../types').AlignmentSlice> = (set) => ({
    alignment: {
        actorIndex: 0,
        actorCount: 0,
        sceneId: "",
        sceneName: "",
        actorName: "",
        actorGender: "",
        data: {
            offsetX: 0,
            offsetY: 0,
            offsetZ: 0,
            scale: 1,
            rotation: 0,
            sosBend: 0,
        },
        activeField: 0,
        inputValues: alignmentFields.map(() => ""),
    },
    updateAlignment: (payload: AlignmentPayload) => set((state) => {
        const nextInputValues = alignmentFields.map(f => {
            if (f.key === 'actor') {
                const count = Math.max(payload.actorCount, 1);
                const display = Math.min(payload.actorIndex + 1, count);
                return display.toString();
            }
            const val = payload.data[f.key];
            return f.precision !== undefined ? val.toFixed(f.precision) : val.toString();
        });

        return {
            alignment: {
                ...state.alignment,
                actorIndex: payload.actorIndex,
                actorCount: payload.actorCount,
                sceneId: payload.sceneId,
                sceneName: payload.sceneName,
                actorName: payload.actorName,
                actorGender: payload.actorGender,
                data: { ...payload.data },
                inputValues: nextInputValues
            }
        };
    }),
    moveAlignmentField: (direction: number) => set((state) => {
        const fieldCount = alignmentFields.length;
        const next = Math.max(0, Math.min(state.alignment.activeField + direction, fieldCount));
        return { alignment: { ...state.alignment, activeField: next } };
    }),
    setAlignmentActiveField: (index: number) => set((state) => ({
        alignment: { ...state.alignment, activeField: index }
    })),
    updateAlignmentField: ({index, value}) => set((state) => {
        const fieldConfig = alignmentFields[index];
        if (!fieldConfig) return state;

        let minFinal = fieldConfig.min ?? Number.NEGATIVE_INFINITY;
        let maxFinal = fieldConfig.max ?? Number.POSITIVE_INFINITY;

        if (fieldConfig.key === 'actor') {
             const effectiveCount = Math.max(state.alignment.actorCount, 1);
             minFinal = 1;
             maxFinal = effectiveCount;
        }

        const clampedValue = Math.max(minFinal, Math.min(value, maxFinal));
        
        const nextInputValues = [...state.alignment.inputValues];
        nextInputValues[index] = fieldConfig.precision !== undefined
            ? value.toFixed(fieldConfig.precision)
            : Math.round(value).toString();

        const { activeField, inputValues, ...baseAlignment } = state.alignment;
        
        const newAlignmentPayload: AlignmentPayload = {
            ...baseAlignment,
            data: { ...state.alignment.data }
        };

        if (fieldConfig.key === 'actor') {
            // UI is 1-based, internal is 0-based
            newAlignmentPayload.actorIndex = Math.round(clampedValue) - 1;
        } else {
            newAlignmentPayload.data = {
                ...newAlignmentPayload.data,
                [fieldConfig.key]: clampedValue
            };
        }

        console.log("Updating alignment field", fieldConfig.key, clampedValue, newAlignmentPayload.actorIndex);

        if(typeof value === 'number' && !isNaN(value)) {
            window.sendAction?.(JSON.stringify({
                action: fieldConfig.key === 'actor' ? 'alignmentSelectActor' : 'alignmentSet',
                payload: fieldConfig.key === 'actor' ? { actorIndex: newAlignmentPayload.actorIndex } : { actorIndex: newAlignmentPayload.actorIndex, field: fieldConfig.key, value: clampedValue }
            }));
        }

        console.log("Updated alignment field in state", {
                ...state.alignment,
                ...newAlignmentPayload,
                inputValues: nextInputValues,
            });

        return {
            alignment: {
                ...state.alignment,
                ...newAlignmentPayload,
                inputValues: nextInputValues,
            }
        };
    }),
});

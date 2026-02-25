import { AlignmentData } from "./types";

export type AlignmentFieldKey = "actor" | keyof AlignmentData;

export const alignmentFields: Array<{
  key: AlignmentFieldKey;
  label: string;
  precision?: number;
  step?: number;
  min?: number;
  max?: number;
}> = [
  { key: "actor", label: "Actor", step: 1, min: 1 },
  { key: "offsetX", label: "Offset X", precision: 1, step: 0.5 },
  { key: "offsetY", label: "Offset Y", precision: 1, step: 0.5 },
  { key: "offsetZ", label: "Offset Z", precision: 1, step: 0.5 },
  { key: "scale", label: "Scale", precision: 2, step: 0.01 },
  { key: "rotation", label: "Rotation", step: 1, min: -360, max: 360 },
  { key: "sosBend", label: "SOS Bend", step: 1, min: -9, max: 9 },
];

export const alignmentStepMap: Record<keyof AlignmentData, number> = {
  offsetX: 0.5,
  offsetY: 0.5,
  offsetZ: 0.5,
  scale: 0.01,
  rotation: 1,
  sosBend: 1
};

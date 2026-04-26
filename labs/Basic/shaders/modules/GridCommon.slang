module GridCommon;

// How far from the camera is the grid visible
public static float gridSize = 100.0;
// Size of one grid cell (same units as gridSize)
public static float gridCellSize = 0.025;
// Regular thin lines
public static float4 gridColorThin = float4(0.7, 0.7, 0.7, 1.0);
// Thick lines
public static float4 gridColorThick = float4(0.8, 0.8, 0.8, 1.0);

// Minimum number of pixels between grid cells, to switch LOD
public static const float gridMinPixelsBetweenCells = 2.0;

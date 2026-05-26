/**
 * A reactive connection line.
 */
export const Connection = (props) => {
  return (
    <line
      x1={props.fromPos?.x + props.fromSize / 2}
      y1={props.fromPos?.y + props.fromSize / 2}
      x2={props.toPos?.x + props.toSize / 2}
      y2={props.toPos?.y + props.toSize / 2}
      stroke={props.color}
      stroke-width={props.strokeWidth || '2'}
      stroke-opacity={props.opacity}
      stroke-dasharray={props.dashed ? '4 4' : ''}
    />
  );
};

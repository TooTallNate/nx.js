import initYoga from 'yoga-wasm-web/asm';

const ctx = Switch.screen.getContext('2d');

const Yoga = initYoga();
const {
	Node,
	ALIGN_CENTER,
	JUSTIFY_CENTER,
	FLEX_DIRECTION_ROW,
	FLEX_DIRECTION_COLUMN,
	DIRECTION_LTR,
} = Yoga;

const root = Node.create();
root.setWidth(Switch.screen.width);
root.setHeight(Switch.screen.height);
root.setJustifyContent(JUSTIFY_CENTER);
root.setAlignItems(ALIGN_CENTER);
root.setFlexDirection(FLEX_DIRECTION_COLUMN);

const node1 = Node.create();
node1.setWidth(100);
node1.setHeight(100);

const node2 = Node.create();
node2.setWidth(200);
node2.setHeight(150);

root.insertChild(node1, 0);
root.insertChild(node2, 1);

root.calculateLayout(1280, 720, DIRECTION_LTR);
//console.log(root.getComputedLayout());
// {left: 0, top: 0, width: 500, height: 300}

const node1Layout = node1.getComputedLayout();
//console.log(node1Layout);
ctx.fillStyle = 'red';
ctx.fillRect(
	node1Layout.left,
	node1Layout.top,
	node1Layout.width,
	node1Layout.height
);

const node2Layout = node2.getComputedLayout();
//console.log(node2Layout);
ctx.fillStyle = 'green';
ctx.fillRect(
	node2Layout.left,
	node2Layout.top,
	node2Layout.width,
	node2Layout.height
);

import { el, Renderer } from "@elemaudio/core";

const core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

const [left, right] = el.mc.sampleseq(
  {
    path: "testtone",
    channels: 2,
		duration: 4,
    seq: [
      { time: 0, value: 1 },
      { time: 2, value: 0 },
      { time: 3, value: 1 },
    ],
  },
	el.mul(el.phasor(0.2), 5)
);
const stats = core.render(left, right);

console.log(stats);

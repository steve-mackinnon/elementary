import { el, Renderer } from "@elemaudio/core";

const core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

const [left, right] = el.mc.sampleseq2(
  {
    path: "testtone",
    channels: 2,
		duration: 1,
		stretch: 1,
    shift: -7,
    seq: [
      { time: 0, value: 1 },
      { time: 0.5, value: 0 },
      { time: 0.6, value: 1 },
    ],
  },
	el.phasor(0.7)
);
const stats = core.render(left, right);

console.log(stats);

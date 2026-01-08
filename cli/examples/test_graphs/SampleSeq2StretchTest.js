import { el, Renderer } from "@elemaudio/core";

const core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

const out = el.sampleseq2(
  {
    path: "testtone",
    duration: 4,
    stretch: 2,
    shift: -7,
    seq: [
      { time: 0, value: 1 },
      { time: 2, value: 0 },
      { time: 3, value: 1 },
    ],
  },
  el.mul(el.phasor(0.2), 5)
);
const stats = core.render(out, out);

console.log(stats);

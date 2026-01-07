import { el, Renderer } from "@elemaudio/core";

const core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

const out = el.sampleseq(
  {
    path: "testtone",
    duration: 1,
    seq: [
      { time: 0, value: 1 },
      { time: 0.5, value: 0 },
      { time: 0.6, value: 1 },
    ],
  },
  el.phasor(1)
);
const stats = core.render(out, out);

console.log(stats);

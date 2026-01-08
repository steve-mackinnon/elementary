import { el, Renderer } from "@elemaudio/core";

const core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

const [left, right] = el.mc.sample(
  {
    path: "testtone",
    channels: 2,
    startOffset: 0,
    stopOffset: 0,
    mode: "loop",
    playbackRate: 1.2,
  },
  1,
);
const stats = core.render(left, right);

console.log(stats);

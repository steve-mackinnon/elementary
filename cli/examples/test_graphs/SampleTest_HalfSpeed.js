import { el, Renderer } from "@elemaudio/core";

const core = new Renderer((batch) => {
	__postNativeMessage__(JSON.stringify(batch));
});

const sample = el.sample({path: "testtone", channels: 2}, el.train(1), 0.5);
const stats = core.render(sample, sample);

console.log(stats);

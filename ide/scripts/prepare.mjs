// Copies the shared files the IDE needs into web/: the TON618 engine used to
// run code inside the app (exemples/ton618-lite.js, the same one as the
// playground) and the logo. Run before every build (npm run build does it).
import { copyFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
copyFileSync(join(root, "exemples", "ton618-lite.js"), join(here, "..", "web", "ton618-lite.js"));
copyFileSync(join(root, "exemples", "logo.svg"), join(here, "..", "web", "logo.svg"));
console.log("web/ ready: ton618-lite.js and logo.svg copied");

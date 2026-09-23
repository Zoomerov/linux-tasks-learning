const p = require("./node_modules/express/package.json");

console.log("digraph express {");
console.log("  rankdir=LR;");
console.log("  node [shape=box];");

for (const dep of Object.keys(p.dependencies || {})) {
    console.log(`  "express" -> "${dep}";`);
}

console.log("}");

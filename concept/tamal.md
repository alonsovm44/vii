🟢 IN SCOPE (What tamal Does)
1. Absolute File Fetching
Taco takes a string, downloads a .io file, and puts it in a folder. That is 90% of its job.

taco get colors → Downloads colors.io into ./.taco/colors.io.
2. The "Paste" Contract
Taco guarantees that if you run taco get X, the file will be exactly at ./.taco/X.io, so your paste ".taco/X.io" always works.

3. Execution Shortcut

taco run main.io → Checks if local .io files are missing from .taco/. If they are, it fetches them. Then it invokes the io compiler on main.io.
4. Flat Namespace
No groups, no organizations, no @username/package. Just taco get http. First come, first served on io.io. (Like early RubyGems). This prevents the mental overhead of namespacing.

5. Single Entry Point
When you taco push, Taco looks for exactly one file in your directory: _main.io. It uploads that file, and only that file, to the registry.

🔴 OUT OF SCOPE (The Scope Creep Kill List)
If anyone asks for these features, the answer is "No. Write it in IO."

1. NO Dependency Trees
If http.io requires tcp.io, Taco does NOT automatically go fetch tcp.io.
Why? Because nested dependencies require lockfiles, version resolvers, and tree-flattening algorithms. That's 10,000 lines of C code. If http.io needs tcp.io, the author of http.io puts paste ".taco/tcp.io" inside their _main.io, and the user must run taco get tcp themselves. Keep Taco dumb.

2. NO Build Steps / Compilation
Taco does not compile your code. Taco does not link your code. It literally just runs the command io yourfile.io at the very end. If you want to compile IO to C or Assembly, do that in a separate Makefile or shell script. Taco stays out of the toolchain.

3. NO Configuration Files
There is no taco.io, no taco.json, no Taco.toml. Metadata (author, description) is pushed to io.io via CLI flags (taco push --desc "A color lib") or edited on the website UI. Config files are the root of all evil in package management.

4. NO Versions (Initially)
No semantic versioning (taco get colors@1.2.4). Versions introduce massive resolver complexity.
The IO Way: If you update colors.io and break it, you just pick a new name like colors_v2.io. It forces the ecosystem to stay incredibly stable. If a package works, it never breaks, because it never changes.

5. NO Transitive Caching / Offline Mode
Taco always hits io.io. If the internet is down, you can't fetch packages. Implementing a local, verifiable, hash-checked offline cache is a massive security and technical burden.


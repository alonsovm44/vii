# Deployment Pipeline: Vii vs Bash

## The Task
Build Docker image → Push to registry → SSH to server → Deploy → Health check. A real DevOps workflow.

## Side-by-Side

| Aspect | Bash | Vii |
|--------|------|-----|
| **Lines** | 67 | 58 |
| **Special Characters** | `$"'{}[];\|` and more | Just `+` for concat |
| **Retry Logic** | `for ((i=1; i<=MAX; i++))` — C-style | `while i <= max` — readable |
| **SSH Heredoc** | Quote escaping nightmare | String concatenation |
| **Error Handling** | `\|\| true` to suppress, `set -e` kills script | Check result, decide what to do |
| **Variable Defaults** | `${VAR:-default}` syntax | `if var == ""` — obvious |

## Bash Pain Points

```bash
# Variable expansion hell
VERSION="${2:-$(git describe --tags --always 2>/dev/null || echo 'latest')}"

# C-style for loop just to retry
for ((i=1; i<=MAX_RETRIES; i++)); do
    ...
    if [[ $i -eq MAX_RETRIES ]]; then
        ...
    fi
    sleep $RETRY_DELAY  # Did I quote this? Does it matter?
done

# SSH heredoc — quote escaping is torture
ssh $SSH_OPTS "deploy@$SERVER" << EOF
    docker run -d \
        --name myapp-$ENV \
        -p 8080:8080 \
        -e ENV=$ENV \
        "$REGISTRY:$VERSION"  # Are these expanded locally or remotely?
EOF

# Error suppression that kills your script if you forget
ssh ... || true  # Prevents exit, but why is this needed?
```

## Vii Clarity

```vii
# Default values are just conditionals
if env_ == ""
  env_ = "staging"

# Retry loop is readable
while i <= max_retries
  result = sys push_cmd
  if result == 0
    "Push successful"
    break
  if i == max_retries
    "ERROR: Push failed"
    exit 1

# Build commands with simple string concat
remote_cmds = "docker pull " + registry + ":" + version + "\n"
remote_cmds = remote_cmds + "docker run -d --name myapp-" + env_ + " ..."

# Error handling is explicit
result = sys deploy_cmd
if result != 0
  "ERROR: Deployment failed"
  exit 1
```

## Why Vii Wins

1. **No Quoting Anxiety**: `"$var"` vs `$var` vs `"${var}"` — Bash has 3 meanings. Vii has one: the variable value.
2. **Real Loops**: `while i <= max` instead of C-style `for ((i=1; ...))`.
3. **String Building**: Just use `+`. No heredoc escape nightmares.
4. **Explicit Error Handling**: Check `result` and decide. No `set -e` surprises.
5. **Readable Retry Logic**: The retry pattern is obvious in Vii, hidden in Bash syntax.
6. **Compiles to Binary**: Ship `deploy` as a static executable. No "works on my machine."

## The Killer Feature

**Bash SSH heredoc** (lines 35-50):
```bash
ssh ... << EOF
    docker run -d \
        --name myapp-$ENV \
        -e ENV=$ENV \
        ...
EOF
```

**Vii SSH** (lines 42-55):
```vii
remote_cmds = "docker pull " + registry + ":" + version + "\n"
remote_cmds = remote_cmds + "docker stop myapp-" + env_ + " 2>/dev/null || true\n"
...
deploy_cmd = "ssh " + ssh_opts + " deploy@" + server + " '" + remote_cmds + "'"
```

Vii builds the command as a string. You can see exactly what's happening. Bash mixes local and remote variable expansion in ways that surprise even experts.

## Run It

```bash
# Bash version
chmod +x deploy-bash.sh
./deploy-bash.sh staging v1.2.3

# Vii version (interpreted)
vii deploy.vii staging v1.2.3

# Vii version (compiled — ship this to CI/CD)
vii deploy.vii -o deploy
./deploy production v1.2.3
```

---

**Winner**: Vii by a mile. 58 readable lines vs 67 lines of shell quoting hell. The SSH heredoc alone is worth the switch.

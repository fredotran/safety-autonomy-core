# Devin Repository Rules

## Branch Protection and Workflow

### Pull Request Policy
- **ALWAYS create a pull request** for any changes to the main branch
- Never push directly to main for:
  - New features
  - Bug fixes
  - CI/CD changes
  - Documentation updates
  - Configuration changes
  - Any code changes

### Branch Naming Convention
- Use descriptive branch names:
  - `feat/` for new features
  - `fix/` for bug fixes
  - `ci/` for CI/CD changes
  - `docs/` for documentation updates
  - `refactor/` for code refactoring
  - `chore/` for maintenance tasks

### Workflow
1. Create a new branch from main
2. Make changes on the feature branch
3. Commit changes with descriptive messages
4. Push branch to remote
5. Create a pull request
6. Wait for CI checks to pass
7. Request review if needed
8. Merge pull request after approval

### Commit Message Format
Use conventional commits:
- `feat:` for new features
- `fix:` for bug fixes
- `ci:` for CI/CD changes
- `docs:` for documentation
- `refactor:` for code refactoring
- `chore:` for maintenance tasks
- `test:` for test changes

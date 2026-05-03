# .NET Solution to aeb Migration Status

Upstream: https://github.com/dotnet-architecture/eShopOnWeb.git

## Modules

| Module | Compile | Tests | Notes |
|--------|---------|-------|-------|
| src/BlazorShared | OK | — | Generated .csproj, no deps |
| src/ApplicationCore | OK | — | Generated .csproj, Ardalis + System libs |
| src/Infrastructure | OK | — | Generated .csproj, EF Core + Identity |
| src/BlazorAdmin | OK | — | Generated .csproj, Blazor WASM SDK |
| src/PublicApi | OK | — | Generated .csproj, ASP.NET Core Web SDK |
| src/Web | OK | — | Generated .csproj, ASP.NET Core Web SDK |
| tests/UnitTests | OK | PASS | Generated .csproj, xUnit + NSubstitute |
| tests/IntegrationTests | OK | PASS | Generated .csproj, xUnit + EF InMemory |
| tests/FunctionalTests | OK | PASS | Generated .csproj, xUnit + WebApplicationFactory |

9 modules compile, 3/3 test suites pass. All from generated `.csproj` files.

Skipped: `tests/PublicApiIntegrationTests` — pre-existing ambiguous `Program`
class (references both PublicApi and Web).

## Generated .csproj files

aeb generates `.{name}.generated.csproj` in each source directory from
`.build.ae` declarations. The original `.csproj` files are not used.

DSL setters map to MSBuild XML:
- `sdk("Microsoft.NET.Sdk.Web")` → `<Project Sdk="...">`
- `target_framework("net10.0")` → `<TargetFramework>` (override central default)
- `root_namespace("Foo")` → `<RootNamespace>`
- `nullable()` → `<Nullable>enable</Nullable>`
- `nuget("PackageName")` → `<PackageReference Include="..." />`
- `build.dep("src/Other")` → `<ProjectReference Include="..." />`

NuGet versions come from `Directory.Packages.props` (central package management).

## Mixed framework targeting

The .NET 10 SDK builds `net8.0` targets — it just needs the 8.0 runtime
installed alongside. Individual projects can override via `target_framework()`:

```aether
dotnet.build_project(b) {
    target_framework("net10.0")  // override central net8.0 default
    nuget("SomeNewPackage")
}
```

This enables gradual migration: library projects can move to net10.0 while
Blazor/Web projects stay on net8.0 until API compatibility is confirmed.

## What aeb replaces

- `.sln` solution files → aeb topo-sort
- `.csproj` project files → generated from `.build.ae`
- `<PackageReference>` → `nuget("PackageName")`
- `<ProjectReference>` → `build.dep("src/ModuleName")`
- Project properties → DSL setters

## What remains from .NET

- `Directory.Packages.props` — central NuGet version management
- `dotnet build` / `dotnet test` — compilation and test execution
- MSBuild — project evaluation, NuGet restore, Razor source generators

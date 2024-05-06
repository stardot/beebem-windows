# Contributing

Contributions to BeebEm are welcome and encouraged.

## Making changes

* If you're thinking of writing a new feature or making substantial changes to the code, please first discuss the change you wish to make by raising an issue.

* As maintainer, I (@chrisn) will work with you to develop and integrate the feature.

* Please avoid making commits directly to your copy of the `master` branch. This branch is reserved for aggregating changes from other people, and for mainline development from the maintainers. If you commit to `master`, it's likely that your local fork will diverge from the [stardot/beebem-windows](https://github.com/stardot/beebem-windows) repository.

* Before working on a change, please ensure your local fork is up to date with the code in the [stardot/beebem-windows](https://github.com/stardot/beebem-windows) repository, and create a [feature branch](https://www.atlassian.com/git/tutorials/comparing-workflows/feature-branch-workflow) for your changes.

* I may want to make minor changes to your pull request before merging, so please ensure that the **Allow edits from maintainers** option on your feature branch is enabled.

* Please don't change version numbers or update [CHANGES.md](https://github.com/stardot/beebem-windows/blob/master/CHANGES.md). I'll do that when [preparing a new release](#preparing-a-new-release).

* Please follow the existing coding conventions.

* For commit messages, please follow [these guidelines](https://chris.beams.io/posts/git-commit/), although I'm not fussy about use of imperative mood vs past tense. In particular, please don't use [Conventional Commits](https://www.conventionalcommits.org/) style. I may choose to edit your commit messages for consistency when merging.

* This repository maintains a linear commit history. This means we use a [rebase workflow](https://www.atlassian.com/git/tutorials/rewriting-history/git-rebase) and don't allow merge commits.

* When merging a feature branch, I may choose to squash your commits so that the feature is merged as a single logical change.

### Preparing a new release

* When it's time to publish a new release version, create a single commit on `master` with the following changes only:

  * Increment the version number in [Src/Version.h](https://github.com/stardot/beebem-windows/blob/master/Src/Version.h) and set `VERSION_DATE` to today's date.

  * Increment the following values in [Src/InnoSetup/BeebEmSetup.iss](https://github.com/stardot/beebem-windows/blob/master/Src/InnoSetup/BeebEmSetup.iss): `AppVerName`, `VersionInfoVersion`, and the `HKLM\SOFTWARE\BeebEm\Version` registry key

  * Describe the new features in this release in [CHANGES.md](https://github.com/stardot/beebem-windows/blob/master/CHANGES.md).

  * Ensure any new files that should be distributed with BeebEm are added to [Src/InnoSetup/BeebEmSetup.iss](https://github.com/stardot/beebem-windows/blob/master/Src/InnoSetup/BeebEmSetup.iss) and [Src/ZipFile/MakeZipFile.pl](https://github.com/stardot/beebem-windows/blob/master/Src/ZipFile/MakeZipFile.pl).

* Tag this commit using the form `X.Y` and push the commit using `git push origin master --tags`.

* In GitHub, [create a Release](https://github.com/stardot/beebem-windows/releases/new) from this tag, with the tag name as Release title, i.e., `X.Y`.

* Build the installer executable and zip files and upload them to the GitHub Release page.

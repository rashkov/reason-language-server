open Infix;
open TopTypes;

/**
 * Parse the output of the build command to see which files we should re-process
 */
let rec getAffectedFiles = (root, lines) => switch lines {
  | [] => []
  | [one, ...rest] when Utils.startsWith(one, "ninja: error: ") =>
    [Utils.toUri(root /+ "bsconfig.json"), ...getAffectedFiles(root, rest)]
  | [one, ...rest] when Utils.startsWith(one, "File \"") =>
    switch (Utils.split_on_char('"', String.trim(one))) {
      | [_, name, ..._] => [(root /+ name) |> Utils.toUri, ...getAffectedFiles(root, rest)]
      | _ => {
        Log.log("Unable to parse file line " ++ one);
        getAffectedFiles(root, rest)
      }
    }
  | [one, two, ...rest] when Utils.startsWith(one, "  Warning number ") || Utils.startsWith(one, "  We've found a bug ") =>
    switch (Utils.split_on_char(' ', String.trim(two))) {
      | [one, ..._] => [one |> String.trim |> Utils.toUri, ...getAffectedFiles(root, rest)]
      | _ => getAffectedFiles(root, [two, ...rest])
    }
  | [_, ...rest] => {
    /* Log.log(
      "Not covered " ++ one
    ); */
    getAffectedFiles(root, rest)
  }
};

let runBuildCommand = (~reportDiagnostics, state, root, buildCommand) => {
  /** TODO check for a bsb.lock file & bail if it's there */
  /** TODO refactor so Dune projects don't get bsconfig.json handling below */
  switch buildCommand {
    | None => Ok()
    | Some((buildCommand, commandDirectory)) =>
      Log.log(">> Build system running: " ++ buildCommand);
      let (stdout, stderr, _success) = Commands.execFull(~pwd=commandDirectory, buildCommand);
      Log.log(">>> stdout");
      Log.log(Utils.joinLines(stdout));
      Log.log(">>> stderr");
      let errors = Utils.joinLines(stderr);
      Log.log(errors);
      let%try _ = if (Utils.startsWith(errors, "Error: Could not find an item in the entries field to compile to ")) {
        Error("Bsb-native " ++ errors ++ "\nHint: check your bsconfig's \"entries\".")
      } else {
        Ok();
      };
      let files = getAffectedFiles(commandDirectory, stdout @ stderr);
      Log.log("Affected files: " ++ String.concat(" ", files));
      let bsconfigJson = root /+ "bsconfig.json" |> Utils.toUri;
      let bsconfigClean = ref(true);
      files |. Belt.List.forEach(uri => {
        if (Utils.endsWith(uri, "bsconfig.json")) {
          bsconfigClean := false;
          Log.log("Bsconfig.json sending");
          reportDiagnostics(
            uri, `BuildFailed(stdout @ stderr)
            )
        };
        Hashtbl.remove(state.compiledDocuments, uri);
        Hashtbl.replace(state.documentTimers, uri, Unix.gettimeofday() -. 0.01)
      });
      if (bsconfigClean^) {
        Log.log("Cleaning bsconfig.json");
        reportDiagnostics(bsconfigJson, `BuildSucceeded)
      };
      /* TODO report notifications here */
      Ok()
  }
};

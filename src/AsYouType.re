
type result =
  | ParseError(string)
  | TypeError(string, Cmt_format.cmt_infos)
  | Success(Cmt_format.cmt_infos)
;
open Infix;
open Result;

let runRefmt = (text, rootPath) => {
  let refmt = rootPath /+ "node_modules/bs-platform/lib/refmt.exe";
  let (out, error, success) = Commands.execFull(~input=text, Printf.sprintf("%S --print binary --parse re > /tmp/ls.ast", refmt));
  if (success) {
    Ok("/tmp/ls.ast")
    /* TODO JSX */
    /* let (out, err, success) = Commands.execFull(bsRoot /+ "lib/reactjs_jsx_ppx_2.exe " ++ tmpFile ++ " " ++ tmpFile ++ "2"); */
  } else {
    Error(out @ error)
  }
};

let parseTypeError = text => {
  let rx = Str.regexp("File \"[^\"]+\", line ([0-9]), characters ([0-9])+-([0-9])+:");
  let rx = Str.regexp({|File "[^"]*", line \([0-9]+\), characters \([0-9]+\)-\([0-9]+\):|});
  if (Str.string_match(rx, text, 0)) {
    let line = Str.matched_group(1, text) |> int_of_string;
    let c0 = Str.matched_group(2, text) |> int_of_string;
    let c1 = Str.matched_group(3, text) |> int_of_string;
    Some((line - 1, c0, c1))
  } else {
    None
  }
};

let justBscCommand = (rootPath, sourceFile, includes) => {
  Printf.sprintf(
    {|%s -w -A %s -impl %s|},
    rootPath /+ "node_modules/bs-platform/lib/bsc.exe",
    includes |> List.map(Printf.sprintf("-I %S")) |> String.concat(" "),
    sourceFile
  )
};

let runBsc = (rootPath, sourceFile, includes) => {
  let cmd = justBscCommand(rootPath, sourceFile, includes);
  let (out, error, success) = Commands.execFull(cmd);
  if (success) {
    Ok(())
    /* TODO JSX */
    /* let (out, err, success) = Commands.execFull(bsRoot /+ "lib/reactjs_jsx_ppx_2.exe " ++ tmpFile ++ " " ++ tmpFile ++ "2"); */
  } else {
    Error(out @ error)
  }
};

let process = (text, rootPath, includes) => {
  Log.log("Compiling text " ++ text);
  open InfixResult;
  switch (runRefmt(text, rootPath)) {
  | Error(lines) => ParseError(String.concat("\n", lines))
  | Ok(fname) => switch (runBsc(rootPath, fname, includes)) {
    | Error(lines) => {
      let cmt = Cmt_format.read_cmt("/tmp/ls.cmt");
      TypeError(String.concat("\n", lines), cmt)
    }
    | Ok(()) => {
      let cmt = Cmt_format.read_cmt("/tmp/ls.cmt");
      Success(cmt)
    }
  }
  }
};
{ pkgs }:

let
  version = "c55573b6";
  archive = pkgs.fetchurl {
    name = "PCSX-Redux-${version}-linux-x86_64.zip";
    url = "https://distrib.app/storage/assets/616/76f/63e/4ea4903902b7217a1764813460371799d36d19800643018ce3e1ee8/PCSX-Redux-${version}-linux-x86_64.zip";
    hash = "sha256-GsSW8soWdnloen13QQz62grjo0TYJSDy15wtx/JnqQQ=";
  };

  appImage = pkgs.runCommand "pcsx-redux-${version}-appimage" {
    nativeBuildInputs = [ pkgs.unzip ];
  } ''
    mkdir -p "$out"
    unzip -q ${archive} -d "$out"
  '';

  appImageArgs = {
    pname = "pcsx-redux";
    inherit version;
    src = "${appImage}/PCSX-Redux-HEAD-x86_64.AppImage";
  };
  contents = pkgs.appimageTools.extractType2 appImageArgs;
in
pkgs.appimageTools.wrapType2 (appImageArgs // {
  passthru = {
    inherit contents;
    openbios = "${contents}/usr/share/pcsx-redux/resources/openbios.bin";
  };
  meta = {
    description = "Development-oriented PlayStation emulator";
    homepage = "https://pcsx-redux.consoledev.net/";
    license = pkgs.lib.licenses.gpl2Plus;
    mainProgram = "pcsx-redux";
    platforms = [ "x86_64-linux" ];
    sourceProvenance = [ pkgs.lib.sourceTypes.binaryNativeCode ];
  };
})

{ pkgs }:

let
  version = "6d083a39";
  archive = pkgs.fetchurl {
    name = "PCSX-Redux-${version}-linux-x86_64.zip";
    url = "https://distrib.app/storage/assets/114/810/550/2aabccac1603af19e3fb95178c68bbd8be34a96ae16a6004e49764f/PCSX-Redux-${version}-linux-x86_64.zip";
    hash = "sha256-3eTnXv7saG8T/olU8kljM37VFudpa8yU7yFVIHLIhag=";
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

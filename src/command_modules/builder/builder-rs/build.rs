fn main() -> Result<(), Box<dyn std::error::Error>> {
    tonic_prost_build::configure()
        .type_attribute(
            "tgbot.builder.linuxkernel.Architecture",
            "#[derive(serde::Deserialize, serde::Serialize)]",
        )
        .type_attribute(
            "tgbot.builder.linuxkernel.Architecture",
            "#[serde(rename_all = \"lowercase\")]",
        )
        .file_descriptor_set_path(std::env::var("OUT_DIR").unwrap() + "/descriptor.bin")
        .compile_protos(
            &[
                "../kernel/proto/LinuxKernelBuild_service.proto",
                "../proto/SystemMonitor_service.proto",
                "../android/proto/ROMBuild_service.proto",
            ],
            &["../kernel/proto", "../proto", "../android/proto"],
        )?;
    Ok(())
}

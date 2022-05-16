module github.com/gmofishsauce/yarc

go 1.16

require (
	github.com/spf13/cobra v1.4.0
	yarc/asm v0.0.0-00010101000000-000000000000
	yarc/host v0.0.0-00010101000000-000000000000
	yarc/protogen v0.0.0-00010101000000-000000000000
)

replace yarc/protogen => ./pkg/protogen

replace yarc/host => ./pkg/host

replace yarc/arduino => ./pkg/arduino

replace yarc/proto => ./pkg/proto

replace yarc/asm => ./pkg/asm

module github.com/gmofishsauce/yarc

go 1.16

require (
	github.com/spf13/cobra v1.4.0 // indirect
	yarc/protogen v0.0.0-00010101000000-000000000000 // indirect
)

replace yarc/protogen => ./pkg/protogen

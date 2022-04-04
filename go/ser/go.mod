module github.com/gmofishsauce/yarc/go/ser

go 1.16

replace local.pkg/arduino => ./arduino

require (
	golang.org/x/sys v0.0.0-20220114195835-da31bd327af9 // indirect
	golang.org/x/term v0.0.0-20210927222741-03fcf44c2211 // indirect
	local.pkg/arduino v0.0.0-00010101000000-000000000000
	local.pkg/serial_protocol v0.0.0-00010101000000-000000000000
)

replace local.pkg/serial_protocol => ./serial_protocol

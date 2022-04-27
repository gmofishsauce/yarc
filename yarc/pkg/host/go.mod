module github.com/gmofishsauce/yarc/yarc/pkg/host

go 1.16

require (
	golang.org/x/term v0.0.0-20220411215600-e5f449aeb171
	yarc/arduino v0.0.0-00010101000000-000000000000 // indirect
	yarc/proto v0.0.0-00010101000000-000000000000 // indirect
)

replace yarc/arduino => ../arduino

replace yarc/proto => ../proto

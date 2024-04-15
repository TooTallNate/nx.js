export enum FsSaveDataSpaceId {
	System = 0, ///< System
	User = 1, ///< User
	SdSystem = 2, ///< SdSystem
	Temporary = 3, ///< [3.0.0+] Temporary
	SdUser = 4, ///< [4.0.0+] SdUser
	ProperSystem = 100, ///< [3.0.0+] ProperSystem
	SafeMode = 101, ///< [3.0.0+] SafeMode
}

export enum FsSaveDataType {
	System = 0, ///< System
	Account = 1, ///< Account
	Bcat = 2, ///< Bcat
	Device = 3, ///< Device
	Temporary = 4, ///< [3.0.0+] Temporary
	Cache = 5, ///< [3.0.0+] Cache
	SystemBcat = 6, ///< [4.0.0+] SystemBcat
}

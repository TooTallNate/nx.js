import { def } from '../utils';

/**
 * @see https://developer.mozilla.org/docs/Web/API/HTMLAudioElement
 */
export class Audio {
	//autoplay: boolean;
	//buffered: TimeRanges;
	//controls: boolean;
	//crossOrigin: string | null;
	//currentSrc: string;
	//currentTime: number;
	//defaultMuted: boolean;
	//defaultPlaybackRate: number;
	//disableRemotePlayback: boolean;
	//duration: number;
	//ended: boolean;
	//error: MediaError | null;
	//loop: boolean;
	//mediaKeys: MediaKeys | null;
	//muted: boolean;
	//networkState: number;
	//onencrypted: ((this: HTMLMediaElement, ev: MediaEncryptedEvent) => any) | null;
	//onwaitingforkey: ((this: HTMLMediaElement, ev: Event) => any) | null;
	//paused: boolean;
	//playbackRate: number;
	//played: TimeRanges;
	//preload: "" | "none" | "metadata" | "auto";
	//preservesPitch: boolean;
	//readyState: number;
	//remote: RemotePlayback;
	//seekable: TimeRanges;
	//seeking: boolean;
	src: string | null;
	//srcObject: MediaProvider | null;
	//textTracks: TextTrackList;
	//volume: number;
	//addTextTrack(kind: TextTrackKind, label?: string | undefined, language?: string | undefined): TextTrack {
	//	throw new Error("Method not implemented.");
	//}
	//canPlayType(type: string): CanPlayTypeResult {
	//	throw new Error("Method not implemented.");
	//}
	//fastSeek(time: number): void {
	//	throw new Error("Method not implemented.");
	//}
	//load(): void {
	//	throw new Error("Method not implemented.");
	//}
	//pause(): void {
	//	throw new Error("Method not implemented.");
	//}
	//play(): Promise<void> {
	//	throw new Error("Method not implemented.");
	//}
	//setMediaKeys(mediaKeys: MediaKeys | null): Promise<void> {
	//	throw new Error("Method not implemented.");
	//}
	//NETWORK_EMPTY: 0;
	//NETWORK_IDLE: 1;
	//NETWORK_LOADING: 2;
	//NETWORK_NO_SOURCE: 3;
	//HAVE_NOTHING: 0;
	//HAVE_METADATA: 1;
	//HAVE_CURRENT_DATA: 2;
	//HAVE_FUTURE_DATA: 3;
	//HAVE_ENOUGH_DATA: 4;

	/**
	 *
	 * @param url An optional string containing the URL of an audio file to be associated with the new audio element.
	 * @see https://developer.mozilla.org/docs/Web/API/HTMLAudioElement/Audio
	 */
	constructor(url?: string) {
		this.src = url ?? null;
	}
}
def(Audio);

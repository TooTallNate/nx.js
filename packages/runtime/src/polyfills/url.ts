import 'core-js/actual/url';
import 'core-js/actual/url-search-params';

export const objectUrls = new Map<string, Blob>();

URL.createObjectURL = (obj) => {
	if (!(obj instanceof Blob)) {
		throw new Error('Must be Blob');
	}
	const url = `blob:${crypto.randomUUID()}`;
	objectUrls.set(url, obj);
	return url;
};

URL.revokeObjectURL = (url) => {
	objectUrls.delete(url);
};
